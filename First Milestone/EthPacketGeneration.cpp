//======================================================================================================//
// Name: Ahmed Amr Abdellatif Mahmoud                                                                   //
// Project: DISW 5G Siemens Diploma - First Milestone                                                   //
// Description: Generate a stream of ethernet frames                                                    //
// Last Modification : 9/19/2024 10:10 PM                                                               //
//======================================================================================================//

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <map>

#define ETH_HEADER_SIZE 26
using namespace std;

// Converts a given number into an array of bytes, with the most significant byte at the lowest index.
template <typename intType, uint8_t my_size>
array<uint8_t, my_size> intToArray(intType number)
{
    array<uint8_t, my_size> result;
    uint8_t *byte = reinterpret_cast<uint8_t *>(&number);

    for (char i = my_size - 1; i >= 0; --i)
    {
        result[i] = *byte++;
    }
    return result;
}

class parseConfigurations
{
public:
    // Configuration parameters parsed from the file
    uint8_t LineRate;              // Ethernet line rate (in Gbps)
    uint8_t CaptureSizeMs;         // Capture duration in milliseconds
    uint8_t MinNumOfIFGsPerPacket; // Minimum number of inter-frame gaps (IFGs) per packet
    uint64_t DestAddress;          // Destination MAC address
    uint64_t SourceAddress;        // Source MAC address
    uint16_t MaxPacketSize;        // Maximum packet size (in bytes)
    uint8_t BurstSize;             // Number of packets per burst
    uint32_t BurstPeriodicity_us;  // Burst periodicity in microseconds

    // Constructor to parse configuration values from a file
    parseConfigurations(string fileName)
    {
        // Open the file for reading
        ifstream MyReadFile(fileName);

        // String to hold each line from the configuration file
        string line;

        // Create a map to store key-value pairs from the file
        map<string, uint64_t> config;

        // Read the file line by line
        while (getline(MyReadFile, line))
        {
            // Skip empty lines
            if (line.empty())
            {
                continue;
            }

            // Remove all whitespace characters from the line
            line.erase(remove_if(line.begin(), line.end(), [](unsigned char x)
                                 { return isspace(x); }),
                       line.end());

            // Remove comments (anything after "//")
            auto comment_pos = line.find("//");
            if (comment_pos != string::npos)
            {
                line.erase(line.begin() + line.find("//"), line.end());
            }

            // Split the line at the '=' sign
            auto key = line.substr(0, line.find('='));
            auto valueStr = line.substr(line.find('=') + 1);

            // Convert the value to an appropriate data type (hexadecimal or decimal)
            uint64_t value;
            if (valueStr.find("0x") == 0) // If value is in hexadecimal format
            {
                value = stoull(valueStr, nullptr, 16);
            }
            else
            {
                value = stoull(valueStr); // Convert hex string to unsigned long long
            }

            // Store the key-value pair in the map
            config[key] = value;
        }

        // Set the configuration fields from the map values
        LineRate = static_cast<uint8_t>(config["Eth.LineRate"]);
        CaptureSizeMs = static_cast<uint8_t>(config["Eth.CaptureSizeMs"]);
        MinNumOfIFGsPerPacket = static_cast<uint8_t>(config["Eth.MinNumOfIFGsPerPacket"]);
        DestAddress = config["Eth.DestAddress"];
        SourceAddress = config["Eth.SourceAddress"];
        MaxPacketSize = static_cast<uint16_t>(config["Eth.MaxPacketSize"]);
        BurstSize = static_cast<uint8_t>(config["Eth.BurstSize"]);
        BurstPeriodicity_us = static_cast<uint16_t>(config["Eth.BurstPeriodicity_us"]);

        // Close the file
        MyReadFile.close();
    }
};

class EthFrame
{
private:
    // Ethernet frame preamble (7 bytes) and Start Frame Delimiter (SFD) (1 byte)
    const array<uint8_t, 8> preamble{0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5};

    // Destination MAC address (6 bytes)
    array<uint8_t, 6> destAddress;

    // Source MAC address (6 bytes)
    array<uint8_t, 6> sourceAddress;

    // EtherType/Size field (2 bytes)
    array<uint8_t, 2> etherSize;

    // Payload data (variable size)
    vector<uint8_t> payload;

    // Frame Check Sequence (FCS) (4 bytes)
    array<uint8_t, 4> fcs;

    // Final Ethernet frame as a vector of bytes
    vector<uint8_t> frame;

public:
    // EthFrame Constructor
    EthFrame(const array<uint8_t, 6> &dest, const array<uint8_t, 6> &src, const array<uint8_t, 2> &size, const vector<uint8_t> &data)
        : destAddress(dest), sourceAddress(src), etherSize(size), payload(move(data))
    {
    }

    // Method to calculate CRC-32 (Frame Check Sequence) for the given data
    array<uint8_t, 4> crc32(const vector<uint8_t> &data)
    {
        uint32_t crc = 0xFFFFFFFF;              // Initial CRC value
        const uint32_t polynomial = 0xEDB88320; // Polynomial used in Ethernet CRC-32

        for (auto byte : data)
        {
            crc ^= static_cast<uint32_t>(byte); // XOR byte with current CRC value
            for (int i = 0; i < 8; ++i)
            {
                if (crc & 1)
                {
                    crc = (crc >> 1) ^ polynomial;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        crc = crc ^ 0xFFFFFFFF; // Final XOR to complete the CRC

        // Convert the 32-bit CRC result to a 4-byte array
        array<uint8_t, 4> crcArray;
        crcArray[0] = static_cast<uint8_t>(crc >> 24);
        crcArray[1] = static_cast<uint8_t>(crc >> 16);
        crcArray[2] = static_cast<uint8_t>(crc >> 8);
        crcArray[3] = static_cast<uint8_t>(crc);

        return crcArray;
    }

    // Method to construct the complete Ethernet frame
    vector<uint8_t> constructFrame(uint32_t MinNumOfIFGsPerPacket)
    {
        // Add Destination MAC to the frame
        frame.insert(frame.end(), destAddress.begin(), destAddress.end());

        // Add Source MAC to the frame
        frame.insert(frame.end(), sourceAddress.begin(), sourceAddress.end());

        // Add EtherSize field to the frame
        frame.insert(frame.end(), etherSize.begin(), etherSize.end());

        // Add Payload data to the frame
        frame.insert(frame.end(), payload.begin(), payload.end());

        // Calculate and add Frame Check Sequence (CRC-32) to the frame
        fcs = crc32(frame);
        frame.insert(frame.end(), fcs.begin(), fcs.end());

        // Add Preamble (7 bytes) + Start Frame Delimiter (SFD) (1 byte)
        frame.insert(frame.begin(), preamble.begin(), preamble.end());

        // Insert minimum number of IFGS per packet
        for (uint32_t i = 0; i < MinNumOfIFGsPerPacket; i++)
        {
            frame.push_back(0x07); // Insert IFG byte (0x07)
        }

        // Ensure the frame has 4-byte alignment by padding with IFG bytes
        while (frame.size() % 4)
        {
            frame.push_back(0x07);
        }

        return frame; // Return the constructed Ethernet frame
    }
};

class packetStreaming
{
private:
    // Full packet stream, including bursts and periodic IFGs
    vector<uint8_t> fullPacket;

    // IFGs between bursts
    vector<uint8_t> periodicIFG;

    // Ethernet frame details: destination MAC, source MAC, EtherType/Size, and payload
    array<uint8_t, 6> destAddress;
    array<uint8_t, 6> sourceAddress;
    array<uint8_t, 2> etherSize;
    vector<uint8_t> payload;

    // Configuration parameters
    uint64_t lineRate;             // Line rate in bits per second
    uint64_t captureSize;          // Capture size in milliseconds
    uint8_t burstSize;             // Number of packets per burst
    uint64_t burstPeriodicity;     // Time between bursts in microseconds
    uint8_t minNumOfIFGsPerPacket; // Minimum number of IFGs per packet

public:
    // Constructor to initialize the packetStreaming object with configuration and payload data
    packetStreaming(const parseConfigurations &configuration, vector<uint8_t> data)
    {
        // Assign configuration values to class members
        lineRate = static_cast<uint64_t>(configuration.LineRate);
        captureSize = static_cast<uint64_t>(configuration.CaptureSizeMs);
        minNumOfIFGsPerPacket = configuration.MinNumOfIFGsPerPacket;

        // Convert configuration addresses and size to byte arrays
        destAddress = intToArray<uint64_t, 6>(configuration.DestAddress);
        sourceAddress = intToArray<uint64_t, 6>(configuration.SourceAddress);
        etherSize = intToArray<uint64_t, 2>(configuration.MaxPacketSize - ETH_HEADER_SIZE);

        // Burst size and periodicity from configuration
        burstSize = configuration.BurstSize;
        burstPeriodicity = static_cast<uint64_t>(configuration.BurstPeriodicity_us);

        // Resize payload data to match the desired size
        data.resize(configuration.MaxPacketSize - ETH_HEADER_SIZE);
        payload = data;
    }

    // Method to construct the full stream of packets and IFGs
    vector<uint8_t> constructStream()
    {
        // Construct an Ethernet frame using the payload, source, and destination MAC addresses
        EthFrame burst{destAddress, sourceAddress, etherSize, payload};
        auto tempFrame = burst.constructFrame(minNumOfIFGsPerPacket);

        // Calculate total transmission in bytes during the capture time
        uint64_t totalTransmisson{(lineRate * captureSize * 1000000) / 8};

        // Calculate the total number of bursts based on capture size and burst periodicity
        uint64_t totalBursts{(captureSize * 1000) / burstPeriodicity};

        // Calculate the burst length in bytes for each burst period
        uint64_t burstLength{totalTransmisson / totalBursts};

        // Calculate the number of IFG bytes per burst after accounting for the packet size
        uint64_t IFGperBurst{burstLength - (burstSize * tempFrame.size())};

        // Fill periodic IFG with the calculated number of bytes (using IFG byte 0x07)
        periodicIFG.assign(IFGperBurst, 0x07);

        // Build the full packet stream
        cout << ".....Start generating the stream....." << endl;
        for (uint64_t i = 0; i < totalBursts; i++)
        {
            // Insert burstSize number of packets in each burst
            for (uint64_t i = 0; i < burstSize; i++)
            {
                fullPacket.insert(fullPacket.end(), tempFrame.begin(), tempFrame.end());
            }
            // Insert periodic IFG after each burst
            fullPacket.insert(fullPacket.end(), periodicIFG.begin(), periodicIFG.end());
        }

        // Output the number of bytes in the generated packet stream
        cout << ".....Done generating....." << endl;
        cout << "Total Bytes Generated: " << uint64_t(fullPacket.size()) << endl;
        cout << "Total Bursts Generated: " << uint64_t(totalBursts) << endl;
        cout << "Burst Size: " << uint64_t(burstSize) << endl;
        cout << "Total Ethernet Frames: " << uint64_t(totalBursts*burstSize) << endl;
        cout << "Ethernet Frame Size (Including IFGs): " << uint64_t(tempFrame.size()) << endl;

        // Return the full packet stream
        return fullPacket;
    }
};

void writePacketStreamToFile(const vector<uint8_t> &fullPacketStream, const string &fileName)
{
    // Create and open a text file to store the generated packet stream
    ofstream MyFile(fileName);

    // Counter to insert a line break after every 4 bytes
    uint8_t counter{0};

    cout << ".....Start exporting stream to the text file....." << endl;
    // Loop through the fullPacketStream and write each byte to the text file
    for (auto byte : fullPacketStream)
    {
        // Write each byte as a 2-digit hex value, ensuring leading zeros
        MyFile << hex << setw(2) << setfill('0') << static_cast<uint64_t>(byte);

        // Insert a newline after every 4 bytes written to the file
        if (counter == 3)
        {
            MyFile << endl;
            counter = 0; // Reset the counter
        }
        else
        {
            counter++;
        }
    }
    cout << ".....Done exporting....." << endl;
    // Close the file after writing is done
    MyFile.close();
}

//==================================================================================//
int main()
{
    // Initialize a vector of payload data (currently with a single byte of 0x00)
    vector<uint8_t> data = {0x00};

    // Vector to store the full packet stream generated later
    vector<uint8_t> fullPacketStream;

    // Parse configuration file "first_milestone.txt" to extract Ethernet settings
    parseConfigurations configuration("first_milestone.txt");

    // Create packetStreaming object using configuration data and payload
    packetStreaming fullStream{configuration, data};

    // Construct the full packet stream with bursts and IFGs
    fullPacketStream = fullStream.constructStream();

    // Call the new function to write the full packet stream to "packets.txt"
    writePacketStreamToFile(fullPacketStream, "packets.txt");

    return 0;
}
