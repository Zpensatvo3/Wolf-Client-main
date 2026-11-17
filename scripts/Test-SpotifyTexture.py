"""Exercise the shipping cover upload/cleanup code with inert GL calls (g++ required)."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "src/modules/visual/spotify_hud.cpp").read_text(encoding="utf-8")
start = source.index("    GLuint artwork_texture =")
end = source.index("\n\tconstexpr ImU32", start)
resources = source[start:end]
start = source.index("\t\t\tauto& texture = artwork_texture;")
end = source.index("\n\t\t\tif (texture)\n", start)
upload = source[start:end]
fixture = r'''
#include "src/modules/visual/spotify_source.hpp"
#include <cassert>
#include <set>
#include <iostream>
#define APIENTRY
using GLuint=unsigned; using GLenum=unsigned; using GLint=int;
constexpr GLenum GL_TEXTURE_2D=1, GL_TEXTURE_BINDING_2D=2, GL_UNPACK_ALIGNMENT=3,
    GL_TEXTURE_WRAP_S=4, GL_TEXTURE_WRAP_T=5, GL_CLAMP_TO_EDGE=6, GL_RGBA=7,
    GL_UNSIGNED_BYTE=8, GL_TEXTURE_MIN_FILTER=9, GL_TEXTURE_MAG_FILTER=10,
    GL_LINEAR_MIPMAP_LINEAR=11, GL_LINEAR=12;
GLuint binding=999, next_id=0;
GLint alignment=8;
int allocations=0, uploads=0, deletions=0;
bool reject_allocation=false;
std::set<GLuint> live;
void glGenTextures(int n, GLuint* out) {
    assert(n==1);
    if (reject_allocation) { *out=0; return; }
    *out=++next_id; ++allocations; live.insert(*out);
}
void glDeleteTextures(int n,const GLuint* value) {
    assert(n==1 && live.erase(*value)==1); ++deletions;
}
void glGetIntegerv(GLenum key,GLint* out) { *out=key==GL_TEXTURE_BINDING_2D ? binding : alignment; }
void glBindTexture(GLenum,GLuint value) { binding=value; }
void glPixelStorei(GLenum,GLint value) { alignment=value; }
void glTexParameteri(GLenum,GLenum,GLint) {}
void glTexImage2D(GLenum,int,int,int w,int h,int,GLenum,GLenum,const void* data) {
    assert(live.count(binding)==1 && alignment==1 && w>0 && h>0 && data); ++uploads;
}
void* wglGetProcAddress(const char*) { return nullptr; }
RESOURCES
void update(const modules::spotify::now_playing& info) {
UPLOAD
}
int main() {
    modules::spotify::now_playing info;
    info.artwork=std::vector<unsigned char>(64,127);
    info.artwork_width=info.artwork_height=4; info.artwork_revision=1;
    update(info);
    assert(allocations==1 && uploads==1 && live.size()==1 && binding==999 && alignment==8);
    for(int frame=0;frame<100000;++frame) update(info);
    assert(uploads==1 && allocations==1);
    for(int track=0;track<10000;++track) { ++info.artwork_revision; update(info); }
    assert(allocations==1 && live.size()==1 && uploads==10001 && binding==999 && alignment==8);
    release_artwork_texture(); release_artwork_texture();
    assert(live.empty() && deletions==1);
    update(info); assert(live.size()==1 && allocations==2);
    auto empty=info; empty.artwork.clear(); update(empty); assert(live.empty());
    reject_allocation=true; update(info); assert(live.empty());
    reject_allocation=false; update(info); assert(live.size()==1 && allocations==3);
    release_artwork_texture(); assert(live.empty());
    std::cout << "PASS Spotify GL doubles: 100000 frames, 10000 track changes, one texture, state restoration, cleanup/retry\n";
}
'''.replace("RESOURCES", resources).replace("UPLOAD", upload)
assert "void modules::spotify_hud::on_render_shutdown()" in source
assert "if (!enabled) { release_artwork_texture(); return; }" in source
with tempfile.TemporaryDirectory(prefix="onestep-spotify-texture-") as directory:
    cpp = Path(directory) / "texture_test.cpp"
    exe = Path(directory) / "texture_test"
    cpp.write_text(fixture, encoding="utf-8")
    subprocess.run([shutil.which("g++") or "g++", "-std=c++20", "-I", str(ROOT), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
