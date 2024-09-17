#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <cstdint>
#define ETH_HEADER_SIZE 26
using namespace std;

class EthFrame
{
private:
    const array<uint8_t, 8> preamble = {0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5};
    array<uint8_t, 6> destAddress;
    array<uint8_t, 6> sourceAddress;
    array<uint8_t, 2> etherSize;
    vector<uint8_t> payload;
    array<uint8_t, 4> fcs;

public:
    // Constructor
    EthFrame(const array<uint8_t, 6> &dest, const array<uint8_t, 6> &src, const array<uint8_t, 2> &size, const vector<uint8_t> &data)
        : destAddress(dest), sourceAddress(src), etherSize(size), payload(move(data))
    {}
    // FCS Calculation
    array<uint8_t, 4> crc32(const std::vector<uint8_t> &data)
    {
        uint32_t crc = 0xFFFFFFFF;
        for (auto byte : data)
        {
            crc ^= static_cast<uint32_t>(byte);
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 1)
                {
                    crc = (crc >> 1) ^ 0xEDB88320;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        crc = crc ^ 0xFFFFFFFF;

        array<uint8_t, 4> crcArray;
        crcArray[0] = static_cast<uint8_t>(crc >> 24);
        crcArray[1] = static_cast<uint8_t>(crc >> 16);
        crcArray[2] = static_cast<uint8_t>(crc >> 8);
        crcArray[3] = static_cast<uint8_t>(crc);

        return crcArray;
    }

    // Method to construct the Ethernet frame as a byte stream
    vector<uint8_t> constructFrame()
    {
        vector<uint8_t> frame;

        // Add Destination MAC
        frame.insert(frame.end(), destAddress.begin(), destAddress.end());

        // Add Source MAC
        frame.insert(frame.end(), sourceAddress.begin(), sourceAddress.end());

        // Add etherSize
        frame.insert(frame.end(), etherSize.begin(), etherSize.end());

        // Add Payload
        frame.insert(frame.end(), payload.begin(), payload.end());

        // Add Frame Check Sequence
        fcs = crc32(frame);
        frame.insert(frame.end(), fcs.begin(), fcs.end());

        // Add Preamble (7 bytes) + Start Frame Delimiter (SFD) (1 byte)
        frame.insert(frame.begin(), preamble.begin(), preamble.end());

        return frame;
    }
};

int main()
{

    array<uint8_t, 6> destAddress = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    array<uint8_t, 6> sourceAddress = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33};
    array<uint8_t, 2> size = {0x05, 0xdc};
    vector<uint8_t> data = {0x0};

    uint16_t payloadSize = (static_cast<uint16_t>(size[0]) << 8) | size[1];
    data.resize(payloadSize);

    EthFrame frame_1(destAddress, sourceAddress, size, data);

    cout << "\nFull Frame: ";
    for (auto byte : frame_1.constructFrame()) {
        cout << hex << setw(2) << setfill('0') << static_cast<int>(byte) << "";
    }
    cout << "\n";
    cout << endl;

    return 0;
}