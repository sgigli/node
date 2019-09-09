# Node.js 13 ChangeLog

<!--lint disable prohibited-strings-->
<!--lint disable maximum-line-length-->

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#13.0.0">13.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [11.x](CHANGELOG_V11.md)
  * [10.x](CHANGELOG_V10.md)
  * [9.x](CHANGELOG_V9.md)
  * [8.x](CHANGELOG_V8.md)
  * [7.x](CHANGELOG_V7.md)
  * [6.x](CHANGELOG_V6.md)
  * [5.x](CHANGELOG_V5.md)
  * [4.x](CHANGELOG_V4.md)
  * [0.12.x](CHANGELOG_V012.md)
  * [0.10.x](CHANGELOG_V010.md)
  * [io.js](CHANGELOG_IOJS.md)
  * [Archive](CHANGELOG_ARCHIVE.md)

<a id="13.0.0"></a>
## 2019-10-22, Version 13.0.0 (Current), @BethGriggs

### Notable Changes

* TBD

### Semver-Major Commits

* [[`0376b5b7ba`](https://github.com/nodejs/node/commit/0376b5b7ba)] - **(SEMVER-MAJOR)** **benchmark**: use test/common/tmpdir consistently (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`37317e60b9`](https://github.com/nodejs/node/commit/37317e60b9)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`66eaeac1df`](https://github.com/nodejs/node/commit/66eaeac1df)] - **(SEMVER-MAJOR)** **build**: reset embedder string to "-node.0" (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`d05668d688`](https://github.com/nodejs/node/commit/d05668d688)] - **(SEMVER-MAJOR)** **child_process**: runtime deprecate \_channel (cjihrig) [#27949](https://github.com/nodejs/node/pull/27949)
* [[`4f9cd2770a`](https://github.com/nodejs/node/commit/4f9cd2770a)] - **(SEMVER-MAJOR)** **child_process**: simplify spawn argument parsing (cjihrig) [#27854](https://github.com/nodejs/node/pull/27854)
* [[`66043e1812`](https://github.com/nodejs/node/commit/66043e1812)] - **(SEMVER-MAJOR)** **console**: display timeEnd with suitable time unit (Xavier Stouder) [#29251](https://github.com/nodejs/node/pull/29251)
* [[`d93c3e3350`](https://github.com/nodejs/node/commit/d93c3e3350)] - **(SEMVER-MAJOR)** **deps**: V8: update postmortem metadata generation script (cjihrig) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`e31f0a7d25`](https://github.com/nodejs/node/commit/e31f0a7d25)] - **(SEMVER-MAJOR)** **deps**: update V8 to 7.7.299.4 (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`84d3243ce9`](https://github.com/nodejs/node/commit/84d3243ce9)] - **(SEMVER-MAJOR)** **deps**: V8: cherry-pick b33af60 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`2dcc3665ab`](https://github.com/nodejs/node/commit/2dcc3665ab)] - **(SEMVER-MAJOR)** **deps**: update V8 to 7.6.303.28 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`80d9b1c712`](https://github.com/nodejs/node/commit/80d9b1c712)] - **(SEMVER-MAJOR)** **doc**: wrap long line (cjihrig) [#27951](https://github.com/nodejs/node/pull/27951)
* [[`43a5170858`](https://github.com/nodejs/node/commit/43a5170858)] - **(SEMVER-MAJOR)** **domain**: error handler runs outside of its domain (Julien Gilli) [#26211](https://github.com/nodejs/node/pull/26211)
* [[`5e3b4d6ed9`](https://github.com/nodejs/node/commit/5e3b4d6ed9)] - **(SEMVER-MAJOR)** **fs**: allow int64 offset in fs.write/writeSync/fd.write (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`a3c0014e73`](https://github.com/nodejs/node/commit/a3c0014e73)] - **(SEMVER-MAJOR)** **fs**: use IsSafeJsInt instead of IsNumber for ftruncate (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`0bbda5e5ae`](https://github.com/nodejs/node/commit/0bbda5e5ae)] - **(SEMVER-MAJOR)** **fs**: allow int64 offset in fs.read/readSync/fd.read (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`eadc3850fe`](https://github.com/nodejs/node/commit/eadc3850fe)] - **(SEMVER-MAJOR)** **fs**: close file descriptor of promisified truncate (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`0daec61b9b`](https://github.com/nodejs/node/commit/0daec61b9b)] - **(SEMVER-MAJOR)** **http**: replace superfluous connection property with getter/setter (Robert Nagy) [#29015](https://github.com/nodejs/node/pull/29015)
* [[`461bf36d70`](https://github.com/nodejs/node/commit/461bf36d70)] - **(SEMVER-MAJOR)** **http**: fix test where aborted should not be emitted (Robert Nagy) [#20077](https://github.com/nodejs/node/pull/20077)
* [[`d5577f0395`](https://github.com/nodejs/node/commit/d5577f0395)] - **(SEMVER-MAJOR)** **http**: remove default 'timeout' listener on upgrade (Luigi Pinca) [#26030](https://github.com/nodejs/node/pull/26030)
* [[`c30ef3cbd2`](https://github.com/nodejs/node/commit/c30ef3cbd2)] - **(SEMVER-MAJOR)** **http, http2**: remove default server timeout (Ali Ijaz Sheikh) [#27558](https://github.com/nodejs/node/pull/27558)
* [[`4e782c9deb`](https://github.com/nodejs/node/commit/4e782c9deb)] - **(SEMVER-MAJOR)** **http2**: remove security revert flags (Anna Henningsen) [#29141](https://github.com/nodejs/node/pull/29141)
* [[`41637a530e`](https://github.com/nodejs/node/commit/41637a530e)] - **(SEMVER-MAJOR)** **http2**: remove callback-based padding (Anna Henningsen) [#29144](https://github.com/nodejs/node/pull/29144)
* [[`91a4cb7175`](https://github.com/nodejs/node/commit/91a4cb7175)] - **(SEMVER-MAJOR)** **lib**: rename validateInteger to validateSafeInteger (Zach Bjornson) [#26572](https://github.com/nodejs/node/pull/26572)
* [[`1432065e9d`](https://github.com/nodejs/node/commit/1432065e9d)] - **(SEMVER-MAJOR)** **lib**: correct error.errno to always be numeric (Joyee Cheung) [#28140](https://github.com/nodejs/node/pull/28140)
* [[`702331be90`](https://github.com/nodejs/node/commit/702331be90)] - **(SEMVER-MAJOR)** **lib**: no need to strip BOM or shebang for scripts (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`e2c0c0c680`](https://github.com/nodejs/node/commit/e2c0c0c680)] - **(SEMVER-MAJOR)** **lib**: rework logic of stripping BOM+Shebang from commonjs (Gus Caplan) [#27768](https://github.com/nodejs/node/pull/27768)
* [[`5746769d68`](https://github.com/nodejs/node/commit/5746769d68)] - **(SEMVER-MAJOR)** **lib,test**: fix error message check after V8 update (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`14701e539c`](https://github.com/nodejs/node/commit/14701e539c)] - **(SEMVER-MAJOR)** **module**: runtime deprecate createRequireFromPath() (cjihrig) [#27951](https://github.com/nodejs/node/pull/27951)
* [[`04633eeeb9`](https://github.com/nodejs/node/commit/04633eeeb9)] - **(SEMVER-MAJOR)** **readline**: error on falsy values for callback (Sam Roberts) [#28109](https://github.com/nodejs/node/pull/28109)
* [[`3eea43af07`](https://github.com/nodejs/node/commit/3eea43af07)] - **(SEMVER-MAJOR)** **repl**: close file descriptor of history file (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`5ff00dbdbc`](https://github.com/nodejs/node/commit/5ff00dbdbc)] - **(SEMVER-MAJOR)** **src**: update v8abbr.h for V8 7.7 (cjihrig) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`4b7be335b9`](https://github.com/nodejs/node/commit/4b7be335b9)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 78 (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`a0e2c6d284`](https://github.com/nodejs/node/commit/a0e2c6d284)] - **(SEMVER-MAJOR)** **src**: add error codes to errors thrown in C++ (Yaniv Friedensohn) [#27700](https://github.com/nodejs/node/pull/27700)
* [[`94e980c9d3`](https://github.com/nodejs/node/commit/94e980c9d3)] - **(SEMVER-MAJOR)** **src**: use non-deprecated overload of V8::SetFlagsFromString (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`655e0dc01a`](https://github.com/nodejs/node/commit/655e0dc01a)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 77 (Michaël Zasso) [#28016](https://github.com/nodejs/node/pull/28016)
* [[`e3cd79ef8e`](https://github.com/nodejs/node/commit/e3cd79ef8e)] - **(SEMVER-MAJOR)** **src**: update NODE\_MODULE\_VERSION to 74 (Refael Ackermann) [#27375](https://github.com/nodejs/node/pull/27375)
* [[`eba348b6ae`](https://github.com/nodejs/node/commit/eba348b6ae)] - **(SEMVER-MAJOR)** **src**: make process.env.TZ setter clear tz cache (Ben Noordhuis) [#20026](https://github.com/nodejs/node/pull/20026)
* [[`f2061930c8`](https://github.com/nodejs/node/commit/f2061930c8)] - **(SEMVER-MAJOR)** **src**: enable V8's WASM trap handlers (Gus Caplan) [#27246](https://github.com/nodejs/node/pull/27246)
* [[`b03845b937`](https://github.com/nodejs/node/commit/b03845b937)] - **(SEMVER-MAJOR)** **stream**: make finished call the callback if the stream is closed (Robert Nagy) [#28748](https://github.com/nodejs/node/pull/28748)
* [[`db706da235`](https://github.com/nodejs/node/commit/db706da235)] - **(SEMVER-MAJOR)** **stream**: disallow stream methods on finished stream (Robert Nagy) [#28687](https://github.com/nodejs/node/pull/28687)
* [[`188896ea3e`](https://github.com/nodejs/node/commit/188896ea3e)] - **(SEMVER-MAJOR)** **stream**: do not emit after 'error' (Robert Nagy) [#28708](https://github.com/nodejs/node/pull/28708)
* [[`4a2bd69db9`](https://github.com/nodejs/node/commit/4a2bd69db9)] - **(SEMVER-MAJOR)** **stream**: fix destroy() behavior (Robert Nagy) [#29058](https://github.com/nodejs/node/pull/29058)
* [[`824dc576db`](https://github.com/nodejs/node/commit/824dc576db)] - **(SEMVER-MAJOR)** **stream**: simplify `.pipe()` and `.unpipe()` in Readable (Weijia Wang) [#28583](https://github.com/nodejs/node/pull/28583)
* [[`5a56940d2c`](https://github.com/nodejs/node/commit/5a56940d2c)] - **(SEMVER-MAJOR)** **test**: update postmortem metadata test for V8 7.7 (cjihrig) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`8ef68e66d0`](https://github.com/nodejs/node/commit/8ef68e66d0)] - **(SEMVER-MAJOR)** **test**: clean tmpdir on process exit (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`d3f20a4725`](https://github.com/nodejs/node/commit/d3f20a4725)] - **(SEMVER-MAJOR)** **test**: use unique tmpdirs for each test (João Reis) [#28858](https://github.com/nodejs/node/pull/28858)
* [[`c206e7490c`](https://github.com/nodejs/node/commit/c206e7490c)] - **(SEMVER-MAJOR)** **tools**: sync gypfiles with V8 7.7 (Michaël Zasso) [#28918](https://github.com/nodejs/node/pull/28918)
* [[`9f71dbc334`](https://github.com/nodejs/node/commit/9f71dbc334)] - **(SEMVER-MAJOR)** **util**: include reference anchor for circular structures (Ruben Bridgewater) [#27685](https://github.com/nodejs/node/pull/27685)

### Semver-Minor Commits

* TBD

### Semver-Patch Commits

* TBD
