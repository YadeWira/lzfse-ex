# codec_check

`codec_check.c`, `codec_check.h` and `adapter_lzfse.c` come from zpaq-std's
pre-bundling test harness (zpaq-std, MIT license; see the header of
`codec_check.c`), adapted only in that the adapter uses the public `lzfse.h`
API.

It complements the unit tests in `tests/` because it uses buffers allocated at
their exact size and tries several hundred damaged copies of every compressed
block:

- round trip from exact-size buffers,
- determinism of two compressions into differently pre-filled buffers,
- output caps one byte smaller than the result,
- damaged input (bit flips, overwritten bytes, truncated blocks, damaged
  headers), looking for memory errors and hangs.

This is the test that found the 8-byte over-read in
`fse_in_checked_flush64()` fixed in 1.0.2, which the random fuzzing in the
regular test suite did not reach. Run it under the sanitizer build
(`-DENABLE_SANITIZER=address,undefined`) for the memory checks to be active;
CMake registers it as the `codec_check` test and it runs on a few repository
files:

```sh
cmake -S . -B build -DENABLE_SANITIZER=address,undefined
cmake --build build
ctest --test-dir build -R codec_check --output-on-failure
```

Run it by hand on any file:

```sh
build/lzfse_codec_check some_file ...
```

MIT license, as stated in the source file:

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
