#include "node_messaging.h"

#include "async_wrap-inl.h"
#include "debug_utils.h"
#include "memory_tracker-inl.h"
#include "node_contextify.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_process.h"
#include "util-inl.h"

using node::contextify::ContextifyContext;
using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferCreationMode;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::SharedArrayBuffer;
using v8::String;
using v8::Symbol;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;
using v8::WasmModuleObject;

namespace node {
namespace worker {

Message::Message(MallocedBuffer<char>&& buffer)
    : main_message_buf_(std::move(buffer)) {}

bool Message::IsCloseMessage() const {
  return main_message_buf_.data == nullptr;
}

namespace {

// This is used to tell V8 how to read transferred host objects, like other
// `MessagePort`s and `SharedArrayBuffer`s, and make new JS objects out of them.
class DeserializerDelegate : public ValueDeserializer::Delegate {
 public:
  DeserializerDelegate(
      Message* m,
      Environment* env,
      const std::vector<MessagePort*>& message_ports,
      const std::vector<Local<SharedArrayBuffer>>& shared_array_buffers,
      const std::vector<WasmModuleObject::TransferrableModule>& wasm_modules)
      : message_ports_(message_ports),
        shared_array_buffers_(shared_array_buffers),
        wasm_modules_(wasm_modules) {}

  MaybeLocal<Object> ReadHostObject(Isolate* isolate) override {
    // Currently, only MessagePort hosts objects are supported, so identifying
    // by the index in the message's MessagePort array is sufficient.
    uint32_t id;
    if (!deserializer->ReadUint32(&id))
      return MaybeLocal<Object>();
    CHECK_LE(id, message_ports_.size());
    return message_ports_[id]->object(isolate);
  }

  MaybeLocal<SharedArrayBuffer> GetSharedArrayBufferFromId(
      Isolate* isolate, uint32_t clone_id) override {
    CHECK_LE(clone_id, shared_array_buffers_.size());
    return shared_array_buffers_[clone_id];
  }

  MaybeLocal<WasmModuleObject> GetWasmModuleFromId(
      Isolate* isolate, uint32_t transfer_id) override {
    CHECK_LE(transfer_id, wasm_modules_.size());
    return WasmModuleObject::FromTransferrableModule(
        isolate, wasm_modules_[transfer_id]);
  }

  ValueDeserializer* deserializer = nullptr;

 private:
  const std::vector<MessagePort*>& message_ports_;
  const std::vector<Local<SharedArrayBuffer>>& shared_array_buffers_;
  const std::vector<WasmModuleObject::TransferrableModule>& wasm_modules_;
};

}  // anonymous namespace

MaybeLocal<Value> Message::Deserialize(Environment* env,
                                       Local<Context> context) {
  CHECK(!IsCloseMessage());

  EscapableHandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);

  // Create all necessary MessagePort handles.
  std::vector<MessagePort*> ports(message_ports_.size());
  for (uint32_t i = 0; i < message_ports_.size(); ++i) {
    ports[i] = MessagePort::New(env,
                                context,
                                std::move(message_ports_[i]));
    if (ports[i] == nullptr) {
      for (MessagePort* port : ports) {
        // This will eventually release the MessagePort object itself.
        if (port != nullptr)
          port->Close();
      }
      return MaybeLocal<Value>();
    }
  }
  message_ports_.clear();

  std::vector<Local<SharedArrayBuffer>> shared_array_buffers;
  // Attach all transferred SharedArrayBuffers to their new Isolate.
  for (uint32_t i = 0; i < shared_array_buffers_.size(); ++i) {
    Local<SharedArrayBuffer> sab;
    if (!shared_array_buffers_[i]->GetSharedArrayBuffer(env, context)
            .ToLocal(&sab))
      return MaybeLocal<Value>();
    shared_array_buffers.push_back(sab);
  }
  shared_array_buffers_.clear();

  DeserializerDelegate delegate(
      this, env, ports, shared_array_buffers, wasm_modules_);
  ValueDeserializer deserializer(
      env->isolate(),
      reinterpret_cast<const uint8_t*>(main_message_buf_.data),
      main_message_buf_.size,
      &delegate);
  delegate.deserializer = &deserializer;

  // Attach all transferred ArrayBuffers to their new Isolate.
  for (uint32_t i = 0; i < array_buffer_contents_.size(); ++i) {
    if (!env->isolate_data()->uses_node_allocator()) {
      // We don't use Node's allocator on the receiving side, so we have
      // to create the ArrayBuffer from a copy of the memory.
      AllocatedBuffer buf =
          env->AllocateManaged(array_buffer_contents_[i].size);
      memcpy(buf.data(),
             array_buffer_contents_[i].data,
             array_buffer_contents_[i].size);
      deserializer.TransferArrayBuffer(i, buf.ToArrayBuffer());
      continue;
    }

    env->isolate_data()->node_allocator()->RegisterPointer(
        array_buffer_contents_[i].data, array_buffer_contents_[i].size);

    Local<ArrayBuffer> ab =
        ArrayBuffer::New(env->isolate(),
                         array_buffer_contents_[i].release(),
                         array_buffer_contents_[i].size,
                         ArrayBufferCreationMode::kInternalized);
    deserializer.TransferArrayBuffer(i, ab);
  }
  array_buffer_contents_.clear();

  if (deserializer.ReadHeader(context).IsNothing())
    return MaybeLocal<Value>();
  return handle_scope.Escape(
      deserializer.ReadValue(context).FromMaybe(Local<Value>()));
}

void Message::AddSharedArrayBuffer(
    const SharedArrayBufferMetadataReference& reference) {
  shared_array_buffers_.push_back(reference);
}

void Message::AddMessagePort(std::unique_ptr<MessagePortData>&& data) {
  message_ports_.emplace_back(std::move(data));
}

uint32_t Message::AddWASMModule(WasmModuleObject::TransferrableModule&& mod) {
  wasm_modules_.emplace_back(std::move(mod));
  return wasm_modules_.size() - 1;
}

namespace {

MaybeLocal<Function> GetDOMException(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> per_context_bindings;
  Local<Value> domexception_ctor_val;
  if (!GetPerContextExports(context).ToLocal(&per_context_bindings) ||
      !per_context_bindings->Get(context,
                                FIXED_ONE_BYTE_STRING(isolate, "DOMException"))
          .ToLocal(&domexception_ctor_val)) {
    return MaybeLocal<Function>();
  }
  CHECK(domexception_ctor_val->IsFunction());
  Local<Function> domexception_ctor = domexception_ctor_val.As<Function>();
  return domexception_ctor;
}

void ThrowDataCloneException(Local<Context> context, Local<String> message) {
  Isolate* isolate = context->GetIsolate();
  Local<Value> argv[] = {message,
                         FIXED_ONE_BYTE_STRING(isolate, "DataCloneError")};
  Local<Value> exception;
  Local<Function> domexception_ctor;
  if (!GetDOMException(context).ToLocal(&domexception_ctor) ||
      !domexception_ctor->NewInstance(context, arraysize(argv), argv)
           .ToLocal(&exception)) {
    return;
  }
  isolate->ThrowException(exception);
}

// This tells V8 how to serialize objects that it does not understand
// (e.g. C++ objects) into the output buffer, in a way that our own
// DeserializerDelegate understands how to unpack.
class SerializerDelegate : public ValueSerializer::Delegate {
 public:
  SerializerDelegate(Environment* env, Local<Context> context, Message* m)
      : env_(env), context_(context), msg_(m) {}

  void ThrowDataCloneError(Local<String> message) override {
    ThrowDataCloneException(context_, message);
  }

  Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object) override {
    if (env_->message_port_constructor_template()->HasInstance(object)) {
      return WriteMessagePort(Unwrap<MessagePort>(object));
    }

    ThrowDataCloneError(env_->clone_unsupported_type_str());
    return Nothing<bool>();
  }

  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate,
      Local<SharedArrayBuffer> shared_array_buffer) override {
    uint32_t i;
    for (i = 0; i < seen_shared_array_buffers_.size(); ++i) {
      if (PersistentToLocal::Strong(seen_shared_array_buffers_[i]) ==
          shared_array_buffer) {
        return Just(i);
      }
    }

    auto reference = SharedArrayBufferMetadata::ForSharedArrayBuffer(
        env_,
        context_,
        shared_array_buffer);
    if (!reference) {
      return Nothing<uint32_t>();
    }
    seen_shared_array_buffers_.emplace_back(
      Global<SharedArrayBuffer> { isolate, shared_array_buffer });
    msg_->AddSharedArrayBuffer(reference);
    return Just(i);
  }

  Maybe<uint32_t> GetWasmModuleTransferId(
      Isolate* isolate, Local<WasmModuleObject> module) override {
    return Just(msg_->AddWASMModule(module->GetTransferrableModule()));
  }

  void Finish() {
    // Only close the MessagePort handles and actually transfer them
    // once we know that serialization succeeded.
    for (MessagePort* port : ports_) {
      port->Close();
      msg_->AddMessagePort(port->Detach());
    }
  }

  ValueSerializer* serializer = nullptr;

 private:
  Maybe<bool> WriteMessagePort(MessagePort* port) {
    for (uint32_t i = 0; i < ports_.size(); i++) {
      if (ports_[i] == port) {
        serializer->WriteUint32(i);
        return Just(true);
      }
    }

    THROW_ERR_MISSING_MESSAGE_PORT_IN_TRANSFER_LIST(env_);
    return Nothing<bool>();
  }

  Environment* env_;
  Local<Context> context_;
  Message* msg_;
  std::vector<Global<SharedArrayBuffer>> seen_shared_array_buffers_;
  std::vector<MessagePort*> ports_;

  friend class worker::Message;
};

}  // anonymous namespace

Maybe<bool> Message::Serialize(Environment* env,
                               Local<Context> context,
                               Local<Value> input,
                               const TransferList& transfer_list_v,
                               Local<Object> source_port) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);

  // Verify that we're not silently overwriting an existing message.
  CHECK(main_message_buf_.is_empty());

  SerializerDelegate delegate(env, context, this);
  ValueSerializer serializer(env->isolate(), &delegate);
  delegate.serializer = &serializer;

  std::vector<Local<ArrayBuffer>> array_buffers;
  for (uint32_t i = 0; i < transfer_list_v.length(); ++i) {
    Local<Value> entry = transfer_list_v[i];
    // Currently, we support ArrayBuffers and MessagePorts.
    if (entry->IsArrayBuffer()) {
      Local<ArrayBuffer> ab = entry.As<ArrayBuffer>();
      // If we cannot render the ArrayBuffer unusable in this Isolate and
      // take ownership of its memory, copying the buffer will have to do.
      if (!ab->IsDetachable() || ab->IsExternal() ||
          !env->isolate_data()->uses_node_allocator()) {
        continue;
      }
      if (std::find(array_buffers.begin(), array_buffers.end(), ab) !=
          array_buffers.end()) {
        ThrowDataCloneException(
            context,
            FIXED_ONE_BYTE_STRING(
                env->isolate(),
                "Transfer list contains duplicate ArrayBuffer"));
        return Nothing<bool>();
      }
      // We simply use the array index in the `array_buffers` list as the
      // ID that we write into the serialized buffer.
      uint32_t id = array_buffers.size();
      array_buffers.push_back(ab);
      serializer.TransferArrayBuffer(id, ab);
      continue;
    } else if (env->message_port_constructor_template()
                  ->HasInstance(entry)) {
      // Check if the source MessagePort is being transferred.
      if (!source_port.IsEmpty() && entry == source_port) {
        ThrowDataCloneException(
            context,
            FIXED_ONE_BYTE_STRING(env->isolate(),
                                  "Transfer list contains source port"));
        return Nothing<bool>();
      }
      MessagePort* port = Unwrap<MessagePort>(entry.As<Object>());
      if (port == nullptr || port->IsDetached()) {
        ThrowDataCloneException(
            context,
            FIXED_ONE_BYTE_STRING(
                env->isolate(),
                "MessagePort in transfer list is already detached"));
        return Nothing<bool>();
      }
      if (std::find(delegate.ports_.begin(), delegate.ports_.end(), port) !=
          delegate.ports_.end()) {
        ThrowDataCloneException(
            context,
            FIXED_ONE_BYTE_STRING(
                env->isolate(),
                "Transfer list contains duplicate MessagePort"));
        return Nothing<bool>();
      }
      delegate.ports_.push_back(port);
      continue;
    }

    THROW_ERR_INVALID_TRANSFER_OBJECT(env);
    return Nothing<bool>();
  }

  serializer.WriteHeader();
  if (serializer.WriteValue(context, input).IsNothing()) {
    return Nothing<bool>();
  }

  for (Local<ArrayBuffer> ab : array_buffers) {
    // If serialization succeeded, we want to take ownership of
    // (a.k.a. externalize) the underlying memory region and render
    // it inaccessible in this Isolate.
    ArrayBuffer::Contents contents = ab->Externalize();
    ab->Detach();

    CHECK(env->isolate_data()->uses_node_allocator());
    env->isolate_data()->node_allocator()->UnregisterPointer(
        contents.Data(), contents.ByteLength());

    array_buffer_contents_.emplace_back(MallocedBuffer<char>{
        static_cast<char*>(contents.Data()), contents.ByteLength()});
  }

  delegate.Finish();

  // The serializer gave us a buffer allocated using `malloc()`.
  std::pair<uint8_t*, size_t> data = serializer.Release();
  CHECK_NOT_NULL(data.first);
  main_message_buf_ =
      MallocedBuffer<char>(reinterpret_cast<char*>(data.first), data.second);
  return Just(true);
}

void Message::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("array_buffer_contents", array_buffer_contents_);
  tracker->TrackFieldWithSize("shared_array_buffers",
      shared_array_buffers_.size() * sizeof(shared_array_buffers_[0]));
  tracker->TrackField("message_ports", message_ports_);
}

MessagePortData::MessagePortData(MessagePort* owner) : owner_(owner) { }

MessagePortData::~MessagePortData() {
  CHECK_NULL(owner_);
  Disentangle();
}

void MessagePortData::MemoryInfo(MemoryTracker* tracker) const {
  Mutex::ScopedLock lock(mutex_);
  tracker->TrackField("incoming_messages", incoming_messages_);
}

void MessagePortData::AddToIncomingQueue(Message&& message) {
  // This function will be called by other threads.
  Mutex::ScopedLock lock(mutex_);
  incoming_messages_.emplace_back(std::move(message));

  if (owner_ != nullptr) {
    Debug(owner_, "Adding message to incoming queue");
    owner_->TriggerAsync();
  }
}

void MessagePortData::Entangle(MessagePortData* a, MessagePortData* b) {
  CHECK_NULL(a->sibling_);
  CHECK_NULL(b->sibling_);
  a->sibling_ = b;
  b->sibling_ = a;
  a->sibling_mutex_ = b->sibling_mutex_;
}

void MessagePortData::Disentangle() {
  // Grab a copy of the sibling mutex, then replace it so that each sibling
  // has its own sibling_mutex_ now.
  std::shared_ptr<Mutex> sibling_mutex = sibling_mutex_;
  Mutex::ScopedLock sibling_lock(*sibling_mutex);
  sibling_mutex_ = std::make_shared<Mutex>();

  MessagePortData* sibling = sibling_;
  if (sibling_ != nullptr) {
    sibling_->sibling_ = nullptr;
    sibling_ = nullptr;
  }

  // We close MessagePorts after disentanglement, so we enqueue a corresponding
  // message and trigger the corresponding uv_async_t to let them know that
  // this happened.
  AddToIncomingQueue(Message());
  if (sibling != nullptr) {
    sibling->AddToIncomingQueue(Message());
  }
}

MessagePort::~MessagePort() {
  if (data_)
    data_->owner_ = nullptr;
}

MessagePort::MessagePort(Environment* env,
                         Local<Context> context,
                         Local<Object> wrap)
  : HandleWrap(env,
               wrap,
               reinterpret_cast<uv_handle_t*>(&async_),
               AsyncWrap::PROVIDER_MESSAGEPORT),
    data_(new MessagePortData(this)) {
  auto onmessage = [](uv_async_t* handle) {
    // Called when data has been put into the queue.
    MessagePort* channel = ContainerOf(&MessagePort::async_, handle);
    channel->OnMessage();
  };
  CHECK_EQ(uv_async_init(env->event_loop(),
                         &async_,
                         onmessage), 0);
  async_.data = static_cast<void*>(this);

  Local<Value> fn;
  if (!wrap->Get(context, env->oninit_symbol()).ToLocal(&fn))
    return;

  if (fn->IsFunction()) {
    Local<Function> init = fn.As<Function>();
    USE(init->Call(context, wrap, 0, nullptr));
  }

  Debug(this, "Created message port");
}

bool MessagePort::IsDetached() const {
  return data_ == nullptr || IsHandleClosing();
}

void MessagePort::TriggerAsync() {
  if (IsHandleClosing()) return;
  CHECK_EQ(uv_async_send(&async_), 0);
}

void MessagePort::Close(v8::Local<v8::Value> close_callback) {
  Debug(this, "Closing message port, data set = %d", static_cast<int>(!!data_));

  if (data_) {
    // Wrap this call with accessing the mutex, so that TriggerAsync()
    // can check IsHandleClosing() without race conditions.
    Mutex::ScopedLock sibling_lock(data_->mutex_);
    HandleWrap::Close(close_callback);
  } else {
    HandleWrap::Close(close_callback);
  }
}

void MessagePort::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor just throws an error. Unfortunately, we can’t use V8’s
  // ConstructorBehavior::kThrow, as that also removes the prototype from the
  // class (i.e. makes it behave like an arrow function).
  Environment* env = Environment::GetCurrent(args);
  THROW_ERR_CONSTRUCT_CALL_INVALID(env);
}

MessagePort* MessagePort::New(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<MessagePortData> data) {
  Context::Scope context_scope(context);
  Local<FunctionTemplate> ctor_templ = GetMessagePortConstructorTemplate(env);

  // Construct a new instance, then assign the listener instance and possibly
  // the MessagePortData to it.
  Local<Object> instance;
  if (!ctor_templ->InstanceTemplate()->NewInstance(context).ToLocal(&instance))
    return nullptr;
  MessagePort* port = new MessagePort(env, context, instance);
  CHECK_NOT_NULL(port);
  if (data) {
    port->Detach();
    port->data_ = std::move(data);

    // This lock is here to avoid race conditions with the `owner_` read
    // in AddToIncomingQueue(). (This would likely be unproblematic without it,
    // but it's better to be safe than sorry.)
    Mutex::ScopedLock lock(port->data_->mutex_);
    port->data_->owner_ = port;
    // If the existing MessagePortData object had pending messages, this is
    // the easiest way to run that queue.
    port->TriggerAsync();
  }
  return port;
}

MaybeLocal<Value> MessagePort::ReceiveMessage(Local<Context> context,
                                              bool only_if_receiving) {
  Message received;
  {
    // Get the head of the message queue.
    Mutex::ScopedLock lock(data_->mutex_);

    Debug(this, "MessagePort has message");

    bool wants_message = receiving_messages_ || !only_if_receiving;
    // We have nothing to do if:
    // - There are no pending messages
    // - We are not intending to receive messages, and the message we would
    //   receive is not the final "close" message.
    if (data_->incoming_messages_.empty() ||
        (!wants_message &&
         !data_->incoming_messages_.front().IsCloseMessage())) {
      return env()->no_message_symbol();
    }

    received = std::move(data_->incoming_messages_.front());
    data_->incoming_messages_.pop_front();
  }

  if (received.IsCloseMessage()) {
    Close();
    return env()->no_message_symbol();
  }

  if (!env()->can_call_into_js()) return MaybeLocal<Value>();

  return received.Deserialize(env(), context);
}

void MessagePort::OnMessage() {
  Debug(this, "Running MessagePort::OnMessage()");
  HandleScope handle_scope(env()->isolate());
  Local<Context> context = object(env()->isolate())->CreationContext();

  // data_ can only ever be modified by the owner thread, so no need to lock.
  // However, the message port may be transferred while it is processing
  // messages, so we need to check that this handle still owns its `data_` field
  // on every iteration.
  while (data_) {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(context);

    Local<Value> payload;
    if (!ReceiveMessage(context, true).ToLocal(&payload)) break;
    if (payload == env()->no_message_symbol()) break;

    if (!env()->can_call_into_js()) {
      Debug(this, "MessagePort drains queue because !can_call_into_js()");
      // In this case there is nothing to do but to drain the current queue.
      continue;
    }

    Local<Object> event;
    Local<Value> cb_args[1];
    if (!env()->message_event_object_template()->NewInstance(context)
            .ToLocal(&event) ||
        event->Set(context, env()->data_string(), payload).IsNothing() ||
        event->Set(context, env()->target_string(), object()).IsNothing() ||
        (cb_args[0] = event, false) ||
        MakeCallback(env()->onmessage_string(),
                     arraysize(cb_args),
                     cb_args).IsEmpty()) {
      // Re-schedule OnMessage() execution in case of failure.
      if (data_)
        TriggerAsync();
      return;
    }
  }
}

void MessagePort::OnClose() {
  Debug(this, "MessagePort::OnClose()");
  if (data_) {
    data_->owner_ = nullptr;
    data_->Disentangle();
  }
  data_.reset();
}

std::unique_ptr<MessagePortData> MessagePort::Detach() {
  CHECK(data_);
  Mutex::ScopedLock lock(data_->mutex_);
  data_->owner_ = nullptr;
  return std::move(data_);
}


Maybe<bool> MessagePort::PostMessage(Environment* env,
                                     Local<Value> message_v,
                                     const TransferList& transfer_v) {
  Isolate* isolate = env->isolate();
  Local<Object> obj = object(isolate);
  Local<Context> context = obj->CreationContext();

  Message msg;

  // Per spec, we need to both check if transfer list has the source port, and
  // serialize the input message, even if the MessagePort is closed or detached.

  Maybe<bool> serialization_maybe =
      msg.Serialize(env, context, message_v, transfer_v, obj);
  if (data_ == nullptr) {
    return serialization_maybe;
  }
  if (serialization_maybe.IsNothing()) {
    return Nothing<bool>();
  }

  Mutex::ScopedLock lock(*data_->sibling_mutex_);
  bool doomed = false;

  // Check if the target port is posted to itself.
  if (data_->sibling_ != nullptr) {
    for (const auto& port_data : msg.message_ports()) {
      if (data_->sibling_ == port_data.get()) {
        doomed = true;
        ProcessEmitWarning(env, "The target port was posted to itself, and "
                                "the communication channel was lost");
        break;
      }
    }
  }

  if (data_->sibling_ == nullptr || doomed)
    return Just(true);

  data_->sibling_->AddToIncomingQueue(std::move(msg));
  return Just(true);
}

static Maybe<bool> ReadIterable(Environment* env,
                                Local<Context> context,
                                // NOLINTNEXTLINE(runtime/references)
                                TransferList& transfer_list,
                                Local<Value> object) {
  if (!object->IsObject()) return Just(false);

  if (object->IsArray()) {
    Local<Array> arr = object.As<Array>();
    size_t length = arr->Length();
    transfer_list.AllocateSufficientStorage(length);
    for (size_t i = 0; i < length; i++) {
      if (!arr->Get(context, i).ToLocal(&transfer_list[i]))
        return Nothing<bool>();
    }
    return Just(true);
  }

  Isolate* isolate = env->isolate();
  Local<Value> iterator_method;
  if (!object.As<Object>()->Get(context, Symbol::GetIterator(isolate))
      .ToLocal(&iterator_method)) return Nothing<bool>();
  if (!iterator_method->IsFunction()) return Just(false);

  Local<Value> iterator;
  if (!iterator_method.As<Function>()->Call(context, object, 0, nullptr)
      .ToLocal(&iterator)) return Nothing<bool>();
  if (!iterator->IsObject()) return Just(false);

  Local<Value> next;
  if (!iterator.As<Object>()->Get(context, env->next_string()).ToLocal(&next))
    return Nothing<bool>();
  if (!next->IsFunction()) return Just(false);

  std::vector<Local<Value>> entries;
  while (env->can_call_into_js()) {
    Local<Value> result;
    if (!next.As<Function>()->Call(context, iterator, 0, nullptr)
        .ToLocal(&result)) return Nothing<bool>();
    if (!result->IsObject()) return Just(false);

    Local<Value> done;
    if (!result.As<Object>()->Get(context, env->done_string()).ToLocal(&done))
      return Nothing<bool>();
    if (done->BooleanValue(isolate)) break;

    Local<Value> val;
    if (!result.As<Object>()->Get(context, env->value_string()).ToLocal(&val))
      return Nothing<bool>();
    entries.push_back(val);
  }

  transfer_list.AllocateSufficientStorage(entries.size());
  std::copy(entries.begin(), entries.end(), &transfer_list[0]);
  return Just(true);
}

void MessagePort::PostMessage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Object> obj = args.This();
  Local<Context> context = obj->CreationContext();

  if (args.Length() == 0) {
    return THROW_ERR_MISSING_ARGS(env, "Not enough arguments to "
                                       "MessagePort.postMessage");
  }

  if (!args[1]->IsNullOrUndefined() && !args[1]->IsObject()) {
    // Browsers ignore null or undefined, and otherwise accept an array or an
    // options object.
    return THROW_ERR_INVALID_ARG_TYPE(env,
        "Optional transferList argument must be an iterable");
  }

  TransferList transfer_list;
  if (args[1]->IsObject()) {
    bool was_iterable;
    if (!ReadIterable(env, context, transfer_list, args[1]).To(&was_iterable))
      return;
    if (!was_iterable) {
      Local<Value> transfer_option;
      if (!args[1].As<Object>()->Get(context, env->transfer_string())
          .ToLocal(&transfer_option)) return;
      if (!transfer_option->IsUndefined()) {
        if (!ReadIterable(env, context, transfer_list, transfer_option)
            .To(&was_iterable)) return;
        if (!was_iterable) {
          return THROW_ERR_INVALID_ARG_TYPE(env,
              "Optional options.transfer argument must be an iterable");
        }
      }
    }
  }

  MessagePort* port = Unwrap<MessagePort>(args.This());
  // Even if the backing MessagePort object has already been deleted, we still
  // want to serialize the message to ensure spec-compliant behavior w.r.t.
  // transfers.
  if (port == nullptr) {
    Message msg;
    USE(msg.Serialize(env, context, args[0], transfer_list, obj));
    return;
  }

  port->PostMessage(env, args[0], transfer_list);
}

void MessagePort::Start() {
  Debug(this, "Start receiving messages");
  receiving_messages_ = true;
  Mutex::ScopedLock lock(data_->mutex_);
  if (!data_->incoming_messages_.empty())
    TriggerAsync();
}

void MessagePort::Stop() {
  Debug(this, "Stop receiving messages");
  receiving_messages_ = false;
}

void MessagePort::Start(const FunctionCallbackInfo<Value>& args) {
  MessagePort* port;
  ASSIGN_OR_RETURN_UNWRAP(&port, args.This());
  if (!port->data_) {
    return;
  }
  port->Start();
}

void MessagePort::Stop(const FunctionCallbackInfo<Value>& args) {
  MessagePort* port;
  CHECK(args[0]->IsObject());
  ASSIGN_OR_RETURN_UNWRAP(&port, args[0].As<Object>());
  if (!port->data_) {
    return;
  }
  port->Stop();
}

void MessagePort::Drain(const FunctionCallbackInfo<Value>& args) {
  MessagePort* port;
  ASSIGN_OR_RETURN_UNWRAP(&port, args[0].As<Object>());
  port->OnMessage();
}

void MessagePort::ReceiveMessage(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  MessagePort* port = Unwrap<MessagePort>(args[0].As<Object>());
  if (port == nullptr) {
    // Return 'no messages' for a closed port.
    args.GetReturnValue().Set(
        Environment::GetCurrent(args)->no_message_symbol());
    return;
  }

  MaybeLocal<Value> payload =
      port->ReceiveMessage(port->object()->CreationContext(), false);
  if (!payload.IsEmpty())
    args.GetReturnValue().Set(payload.ToLocalChecked());
}

void MessagePort::MoveToContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args[0]->IsObject() ||
      !env->message_port_constructor_template()->HasInstance(args[0])) {
    return THROW_ERR_INVALID_ARG_TYPE(env,
        "First argument needs to be a MessagePort instance");
  }
  MessagePort* port = Unwrap<MessagePort>(args[0].As<Object>());
  CHECK_NOT_NULL(port);

  Local<Value> context_arg = args[1];
  ContextifyContext* context_wrapper;
  if (!context_arg->IsObject() ||
      (context_wrapper = ContextifyContext::ContextFromContextifiedSandbox(
          env, context_arg.As<Object>())) == nullptr) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "Invalid context argument");
  }

  std::unique_ptr<MessagePortData> data;
  if (!port->IsDetached())
    data = port->Detach();

  Context::Scope context_scope(context_wrapper->context());
  MessagePort* target =
      MessagePort::New(env, context_wrapper->context(), std::move(data));
  if (target != nullptr)
    args.GetReturnValue().Set(target->object());
}

void MessagePort::Entangle(MessagePort* a, MessagePort* b) {
  Entangle(a, b->data_.get());
}

void MessagePort::Entangle(MessagePort* a, MessagePortData* b) {
  MessagePortData::Entangle(a->data_.get(), b);
}

Local<FunctionTemplate> GetMessagePortConstructorTemplate(Environment* env) {
  // Factor generating the MessagePort JS constructor into its own piece
  // of code, because it is needed early on in the child environment setup.
  Local<FunctionTemplate> templ = env->message_port_constructor_template();
  if (!templ.IsEmpty())
    return templ;

  Isolate* isolate = env->isolate();

  {
    Local<FunctionTemplate> m = env->NewFunctionTemplate(MessagePort::New);
    m->SetClassName(env->message_port_constructor_string());
    m->InstanceTemplate()->SetInternalFieldCount(1);
    m->Inherit(HandleWrap::GetConstructorTemplate(env));

    env->SetProtoMethod(m, "postMessage", MessagePort::PostMessage);
    env->SetProtoMethod(m, "start", MessagePort::Start);

    env->set_message_port_constructor_template(m);

    Local<FunctionTemplate> event_ctor = FunctionTemplate::New(isolate);
    event_ctor->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "MessageEvent"));
    Local<ObjectTemplate> e = event_ctor->InstanceTemplate();
    e->Set(env->data_string(), Null(isolate));
    e->Set(env->target_string(), Null(isolate));
    env->set_message_event_object_template(e);
  }

  return GetMessagePortConstructorTemplate(env);
}

namespace {

static void MessageChannel(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  Local<Context> context = args.This()->CreationContext();
  Context::Scope context_scope(context);

  MessagePort* port1 = MessagePort::New(env, context);
  MessagePort* port2 = MessagePort::New(env, context);
  MessagePort::Entangle(port1, port2);

  args.This()->Set(context, env->port1_string(), port1->object())
      .Check();
  args.This()->Set(context, env->port2_string(), port2->object())
      .Check();
}

static void InitMessaging(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);

  {
    Local<String> message_channel_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "MessageChannel");
    Local<FunctionTemplate> templ = env->NewFunctionTemplate(MessageChannel);
    templ->SetClassName(message_channel_string);
    target->Set(context,
                message_channel_string,
                templ->GetFunction(context).ToLocalChecked()).Check();
  }

  target->Set(context,
              env->message_port_constructor_string(),
              GetMessagePortConstructorTemplate(env)
                  ->GetFunction(context).ToLocalChecked()).Check();

  // These are not methods on the MessagePort prototype, because
  // the browser equivalents do not provide them.
  env->SetMethod(target, "stopMessagePort", MessagePort::Stop);
  env->SetMethod(target, "drainMessagePort", MessagePort::Drain);
  env->SetMethod(target, "receiveMessageOnPort", MessagePort::ReceiveMessage);
  env->SetMethod(target, "moveMessagePortToContext",
                 MessagePort::MoveToContext);

  {
    Local<Function> domexception = GetDOMException(context).ToLocalChecked();
    target
        ->Set(context,
              FIXED_ONE_BYTE_STRING(env->isolate(), "DOMException"),
              domexception)
        .Check();
  }
}

}  // anonymous namespace

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(messaging, node::worker::InitMessaging)
