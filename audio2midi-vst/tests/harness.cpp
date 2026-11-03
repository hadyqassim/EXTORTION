// Headless test: raw float32 mono 22050 Hz in, note list out.
#include "../Source/BasicPitchEngine.h"
#include <cstdio>
#include <fstream>
#include <iterator>
int main (int argc, char** argv)
{
    if (argc < 6) { std::fprintf (stderr, "usage: harness model.onnx audio.f32 onset frame minFrames\n"); return 1; }
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
    bp::DecodeParams dp; dp.onsetThreshold = (float) atof (argv[3]); dp.frameThreshold = (float) atof (argv[4]); dp.minNoteFrames = atoi (argv[5]);
    auto notes = bp::Engine::decode (post, dp);
    std::printf ("frames %d notes %zu\n", post.nFrames, notes.size());
    for (auto& n : notes) std::printf ("%d %.3f %.3f %.3f\n", n.pitch, n.start, n.end, n.amplitude);
}
