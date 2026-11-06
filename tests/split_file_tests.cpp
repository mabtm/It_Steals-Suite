#include "PracticeMapData.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char **argv)
{
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    std::string error;auto data=practice::DecodeMap(bytes,error);
    assert(data && data->review && data->review->id==6001 && data->review->valid && data->review->category==1);
    assert(data->review->seconds==25 && !data->review->collected.empty());
    std::cout<<"Streaming split metadata golden fixture: native full decoder accepts identity, geometry, category, timing and validation: PASS\n";
}
