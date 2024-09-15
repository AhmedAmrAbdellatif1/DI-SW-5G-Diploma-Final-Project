#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#define ETH_HEADER_SIZE 26

class EthPacket {
  public:
    uint64_t preamble;
    uint64_t destinationAddress : 48;
    uint64_t sourceAddress : 48;
    uint16_t length;
    uint32_t crc;

    EthPacket(uint64_t dest, uint64_t src, uint16_t len)
    {
      preamble = 0xfb555555555555d5;
      destinationAddress = dest;
      sourceAddress = src;
      length = len;
    }
};



int main()
{
    int LineRate = 10;
    int CaptureSizeMs = 10;
    int MinNumOfIFGsPerPacket = 12;
    uint64_t DestAddress = 0x010101010101;
    uint64_t SourceAddress = 0x333333333333;
    int MaxPacketSize = 1500;
    int BurstSize = 3;
    int BurstPeriodicity_us = 100;

    EthPacket packet_1(DestAddress, SourceAddress, MaxPacketSize - ETH_HEADER_SIZE);
    std::cout << packet_1.preamble << std::endl;
    std::cout << packet_1.destinationAddress << std::endl;
    std::cout << packet_1.sourceAddress << std::endl;
    std::cout << packet_1.length << std::endl;
    return 0;
}