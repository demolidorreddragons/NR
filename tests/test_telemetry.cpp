#include "pne/telemetry.hpp"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
int main() {
 const auto dir=std::filesystem::temp_directory_path()/"pne-telemetry-test";
 std::error_code ec; std::filesystem::remove_all(dir,ec);
 pne::telemetry::Writer writer(dir.string().c_str());
 assert(writer.start() && writer.active());
 assert(writer.append_present({1000,1000,0x43555341,1}));
 assert(writer.append_present({1016,1000,0x43555341,0}));
 std::vector<std::uint8_t> rgba={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
 auto bmp=(dir/"frame.bmp").string();
 assert(writer.write_rgba8_bmp(bmp,rgba.data(),2,2,8));
 writer.stop(); assert(!writer.active());
 std::ifstream csv(dir/"frametimes.csv"); std::string s((std::istreambuf_iterator<char>(csv)),{});
 assert(s.find("frametime_ms")!=std::string::npos && s.find("16.000000")!=std::string::npos);
 std::ifstream image(bmp,std::ios::binary); char sig[2]{}; image.read(sig,2);
 assert(sig[0]=='B'&&sig[1]=='M'); image.seekg(0,std::ios::end); assert(image.tellg()==70);
 std::filesystem::remove_all(dir,ec);
}