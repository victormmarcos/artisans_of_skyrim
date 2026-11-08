# Third-Party Notices

Artisans of Skyrim uses the following third-party components. The source
package keeps the pinned source references in `CMakeLists.txt` and the vcpkg
manifest so the corresponding sources can be retrieved for a rebuild.

## MinHook

Copyright (C) 2009-2017 Tsuda Kageyu. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## CommonLibSSE-NG

CommonLibSSE-NG 7.2.0 is linked under GPL-3.0 with the project's Modding
Exception and GPL-3.0 Linking Exception. The corresponding license and
exceptions are available in the pinned CommonLibSSE-NG source tree and at
<https://github.com/alandtse/CommonLibSSE-NG/tree/v7.2.0>. The source package's pinned
FetchContent revision is the corresponding source reference for this build.

## OpenVR (headers)

CommonLibSSE-NG's build fetches the OpenVR headers (pinned commit in
`CMakeLists.txt`). BSD 3-Clause License, copyright Valve Corporation.
<https://github.com/ValveSoftware/openvr/blob/master/LICENSE>.

## DirectXMath and DirectXTK

These Microsoft components are distributed under the MIT License. Copyright
Microsoft Corporation. Full license text:
<https://github.com/microsoft/DirectXMath/blob/main/LICENSE>
and <https://github.com/microsoft/DirectXTK/blob/main/LICENSE>.

## fmt and spdlog

fmt is distributed under the MIT License, copyright Victor Zverovich and the
fmt contributors. spdlog is distributed under the MIT License, copyright Gabi
Melman and the spdlog contributors.

## rapidcsv

rapidcsv is distributed under the BSD 3-Clause License, copyright Kristofer
Berggren.

## toml++

toml++ is distributed under the MIT License. The package's copyright and full
license text are included by the pinned vcpkg source used for this build.

## Interface build tool

The original Artisans interface is compiled with MTASC 1.15 (AS2 / Flash 8).
MTASC is a build-time tool and is not redistributed or required by players.
Source and tool license: <https://sourceforge.net/projects/mtasc/>.
No SkyUI code or artwork is included.
