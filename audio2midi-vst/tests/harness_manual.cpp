// Same as harness.cpp but initialises ONNX Runtime the way the Windows plugin does:
// load the library by path, take OrtGetApiBase, call Ort::InitApi.
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <dlfcn.h>
#include "../Source/BasicPitchEngine.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
int main (int argc, char** argv)
{
    void* h = dlopen (argv[4], RTLD_NOW);
    if (!h) { std::fprintf (stderr, "dlopen failed\n"); return 1; }
    auto getBase = (const OrtApiBase* (*)()) dlsym (h, "OrtGetApiBase");
    Ort::InitApi (getBase()->GetApi (ORT_API_VERSION));

    std::ifstream mf (argv[1], std::ios::binary);
    std::vector<char> model ((std::istreambuf_iterator<char> (mf)), {});
    std::ifstream af (argv[2], std::ios::binary);
    std::vector<char> bytes ((std::istreambuf_iterator<char> (af)), {});
    std::vector<float> audio (bytes.size() / 4);
    std::memcpy (audio.data(), bytes.data(), audio.size() * 4);
    bp::Engine eng; std::string err;
    if (! eng.loadModel (model.data(), model.size(), err)) { std::fprintf (stderr, "load: %s\n", err.c_str()); return 2; }
    bp::Posteriors post;
    if (! eng.runModel (audio, post, err)) { std::fprintf (stderr, "run: %s\n", err.c_str()); return 3; }
    bp::DecodeParams dp; dp.onsetThreshold = 0.6f; dp.frameThreshold = 0.35f; dp.minNoteFrames = 10;
    auto notes = bp::Engine::decode (post, dp);
    std::printf ("manual-init frames %d notes %zu\n", post.nFrames, notes.size());
}
