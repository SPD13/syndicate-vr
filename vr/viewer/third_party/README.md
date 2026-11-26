# Third-party code of the VR viewer

Single-header libraries, unchanged copies; their implementations are
compiled once in `vr/viewer/third_party.cpp`.

| File | Version | Licence | Source |
|---|---|---|---|
| `cgltf.h` | 1.14 | MIT | https://github.com/jkuhlmann/cgltf (tag v1.14) |
| `stb_image.h` | 2.30 | MIT or public domain | https://github.com/nothings/stb (commit f58f558) |

cgltf parses the glTF models of the upgraded look; stb_image decodes their
JPEG and PNG textures.
