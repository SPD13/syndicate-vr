# Third-party code of the VR viewer

Single-header libraries, unchanged copies; their implementations are
compiled once in `vr/viewer/third_party.cpp`.

| File | Version | Licence | Source |
|---|---|---|---|
| `cgltf.h` | 1.14 | MIT | https://github.com/jkuhlmann/cgltf (tag v1.14) |
| `stb_image.h` | 2.30 | MIT or public domain | https://github.com/nothings/stb (commit f58f558) |
| `stb_truetype.h` | 1.28 | MIT or public domain | https://github.com/nothings/stb (master, 2026-11-26) |

cgltf parses the glTF models of the upgraded look; stb_image decodes their
JPEG and PNG textures; stb_truetype makes the signed distance field of the
upgraded look's font.

The font, `vr/viewer/fonts/ShareTechMono-Regular.ttf` (Carrois Type
Design, SIL Open Font License 1.1, `vr/viewer/fonts/OFL.txt`), is from
https://github.com/google/fonts (`ofl/sharetechmono`).
