//======================================================================================================//
// Name: Ahmed Amr Abdellatif Mahmoud                                                                   //
// Project: DISW 5G Siemens Diploma - Second Milestone                                                   //
// Description: //
// Last Modification : //
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
#include <cmath>
#include <random>

#define ETH_HEADER_SIZE 26
#define FRAME_PERIOD_MS 10
#define SCS_PERIODICITY 15
#define SUBFRAME_PER_FRAME 10
#define SYMBOL_PER_SLOT 14
#define RE_PER_RB 14

using namespace std;

// Function to convert a given number into an array of bytes, with the most significant byte at the lowest index.
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

// Function to convert the value to an appropriate data type (hexadecimal or decimal)
uint64_t convertIntoInteger(string str)
{
    if (str.find("0x") == 0) // If value is in hexadecimal format
    {
        return stoull(str, nullptr, 16);
    }
    else
    {
        return stoull(str); // Convert hex string to unsigned long long
    }
}

class parseConfigurations
{
public:
    // Ethernet Configuration parameters parsed from the file
    uint8_t LineRate;              // Ethernet line rate (in Gbps)
    uint8_t CaptureSizeMs;         // Capture duration in milliseconds
    uint8_t MinNumOfIFGsPerPacket; // Minimum number of inter-frame gaps (IFGs) per packet
    uint64_t DestAddress;          // Destination MAC address
    uint64_t SourceAddress;        // Source MAC address
    uint16_t MaxPacketSize;        // Maximum packet size (in bytes)

    // ORAN Configuration parameters parsed from the file
    uint8_t SCS;           // Subcarrier spacing
    uint16_t MaxNrb;       // Maximum no. of resource blocks
    uint16_t NrbPerPacket; // No. of resource blocks per packet
    string PayloadType;    //

    vector<int8_t> iqSamples;

    // Constructor to parse configuration values from a file
    parseConfigurations(string fileName)
    {
        //
        cout << "========= Start Parsing =========" << endl;

        // Open the file for reading
        ifstream MyReadFile(fileName);

        // Check if the file opened successfully
        if (!MyReadFile)
        {
            throw runtime_error("Failed to open file!");
        }

        // String to hold each line from the configuration file
        string line;

        // Create a map to store key-value pairs from the file
        map<string, string> config;

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

            // Store the key-value pair in the map
            config[key] = valueStr;

            cout << key << ": " << valueStr << endl;
        }

        //
        cout << "========= Done Parsing =========" << endl;

        // Close the file
        MyReadFile.close();

        // Set the configuration fields from the map values
        LineRate = static_cast<uint8_t>(convertIntoInteger(config["Eth.LineRate"]));
        CaptureSizeMs = static_cast<uint8_t>(convertIntoInteger(config["Eth.CaptureSizeMs"]));
        MinNumOfIFGsPerPacket = static_cast<uint8_t>(convertIntoInteger(config["Eth.MinNumOfIFGsPerPacket"]));
        DestAddress = convertIntoInteger(config["Eth.DestAddress"]);
        SourceAddress = convertIntoInteger(config["Eth.SourceAddress"]);
        MaxPacketSize = static_cast<uint16_t>(convertIntoInteger(config["Eth.MaxPacketSize"]));

        SCS = static_cast<uint8_t>(convertIntoInteger(config["Oran.SCS"]));
        MaxNrb = static_cast<uint16_t>(convertIntoInteger(config["Oran.MaxNrb"]));
        NrbPerPacket = static_cast<uint16_t>(convertIntoInteger(config["Oran.NrbPerPacket"]));
        PayloadType = config["Oran.PayloadType"];

        iqSamples = parseIQSamples(config["Oran.Payload"]);
    }

    // Parse IQ Samples
    vector<int8_t> parseIQSamples(string fileName)
    {
        // Open the file for reading
        ifstream file(fileName);

        // Check if the file opened successfully
        if (!file)
        {
            throw runtime_error("Failed to open file!");
        }

        // Vectors to store the two arrays of integers
        vector<int8_t> samples;

        // String to hold each line from the file
        string line;

        // Read the file line by line
        while (getline(file, line))
        {
            // Use a stringstream to extract the two numbers from each line
            stringstream ss(line);
            int num1, num2;

            // Extract the two integers
            if (ss >> num1 >> num2)
            {
                samples.push_back(num1);
                samples.push_back(num2);
            }
        }

        // Close the file after reading
        file.close();

        // Return the array
        return samples;
    }
};

class OranPacket
{
private:
    array<uint8_t, 8> header; // 8 bytes for the combined ORAN packet header
    vector<int8_t> iqSamples;
    vector<uint8_t> oranPacket;

public:
    // Constructor for the ORAN packet to initialize the headers
    OranPacket(uint8_t frameId, uint8_t subframeId, uint8_t slotId, uint8_t symbolId,
               uint16_t startPrbu, uint16_t numPrbu, vector<int8_t> data)
    {
        // Packing the first 4 bytes (Common Header)
        header[0] = 0x00;                              // dataDirection (1 bit), payloadVersion (3 bits), filterIndex (4 bits), all set to 0
        header[1] = frameId;                           // frameId (8 bits)
        header[2] = (subframeId << 4) | (slotId >> 2); // Top 4 bits of subframeId, top 6 bits of slotId
        header[3] = (slotId << 6) | (symbolId & 0x3F); // Lower 6 bits of slotId, symbolId (6 bits)

        // Packing the next 4 bytes (Section Header)
        header[4] = 0xFF;                             // sectionId (12 bits) fixed to 4095 (0xFFF)
        header[5] = 0xF0 | ((startPrbu >> 8) & 0x03); // Lower 4 bits of sectionId, top 2 bits of startPrbu
        header[6] = startPrbu & 0xFF;                 // Lower 8 bits of startPrbu

        if (numPrbu == 273)
        {
            header[7] = 0; // numPrbu (8 bits)
        }
        else
        {
            header[7] = numPrbu; // numPrbu (8 bits)
        }
        //
        iqSamples = data;
    }

    // Function to
    vector<uint8_t> getPacket()
    {
        //
        oranPacket.insert(oranPacket.end(), header.begin(), header.end());
        //
        oranPacket.insert(oranPacket.end(), iqSamples.begin(), iqSamples.end());

        return oranPacket;
    }
};

class EcpriPacket
{
private:
    array<uint8_t, 8> header; // 8 bytes for the eCPRI header
    vector<uint8_t> payload;
    vector<uint8_t> ecpriPacket;

public:
    // Constructor to initialize the eCPRI packet header
    EcpriPacket(uint16_t ecpriSeqid, vector<uint8_t> ecpriPayload)
    {
        uint16_t ecpriPayloadSize{static_cast<uint16_t>(size(ecpriPayload))};

        header[0] = 0x00;                           // ecpriVersion (4 bits) + ecpriReserved (3 bits) + ecpriConcatenation (1 bit)
        header[1] = 0x00;                           // ecpriMessage
        header[2] = (ecpriPayloadSize >> 8) & 0xFF; // Upper 8 bits of ecpriPayloadSize
        header[3] = ecpriPayloadSize & 0xFF;        // Lower 8 bits of ecpriPayloadSize
        header[4] = 0x00;                           // Upper 8 bits of ecpriRTCid/ecpriPcid
        header[5] = 0x00;                           // Lower 8 bits of ecpriRTCid/ecpriPcid
        header[6] = (ecpriSeqid >> 8) & 0xFF;       // Upper 8 bits of ecpriSeqid
        header[7] = ecpriSeqid & 0xFF;              // Lower 8 bits of ecpriSeqid

        payload = ecpriPayload;
    }

    // Function to
    vector<uint8_t> getPacket()
    {
        //
        ecpriPacket.insert(ecpriPacket.end(), header.begin(), header.end());
        //
        ecpriPacket.insert(ecpriPacket.end(), payload.begin(), payload.end());

        return ecpriPacket;
    }
};

class EthernetPacket
{
private:
    //
    uint32_t minNumOfIFGsPerPacket;

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
    // EthernetPacket Constructor
    EthernetPacket(const array<uint8_t, 6> &dest, const array<uint8_t, 6> &src, const array<uint8_t, 2> &size, const vector<uint8_t> &data, const uint32_t &MinNumOfIFGsPerPacket)
        : destAddress(dest), sourceAddress(src), etherSize(size), payload(move(data)), minNumOfIFGsPerPacket(MinNumOfIFGsPerPacket)
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
    vector<uint8_t> getPacket()
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
        for (uint32_t i = 0; i < minNumOfIFGsPerPacket; i++)
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
    vector<int8_t> iqSamples;
    vector<uint8_t> silentIFGs;

    // Configuration parameters
    uint64_t lineRate;             // Line rate in bits per second
    uint64_t captureSize;          // Capture size in milliseconds
    uint8_t minNumOfIFGsPerPacket; // Minimum number of IFGs per packet
    uint16_t maxPacketSize;        // Maximum packet size (in bytes)

    // ORAN Configuration parameters parsed from the file
    uint8_t scs;           // Subcarrier spacing
    uint16_t maxNrb;       // Maximum no. of resource blocks
    uint16_t nrbPerPacket; // No. of resource blocks per packet
    string payloadType;    //

    // Ethernet frame details: destination MAC, source MAC, EtherType/Size, and payload
    array<uint8_t, 6> destAddress;
    array<uint8_t, 6> sourceAddress;

    // variables needed for the calculations
    uint64_t totalTransmisson;
    double totalFrames;
    uint64_t packetsPERsymbol;
    uint64_t packetsPERslot;
    uint64_t packetsPERsubframe;
    uint64_t packetsPERframe;
    uint64_t totalPackets;
    uint64_t iqSamplesPERpacket;
    uint64_t totalSamples;
    int64_t IFGsNo;

public:
    // Constructor to initialize the packetStreaming object with configuration and payload data
    packetStreaming(const parseConfigurations &configuration)
    {
        // Assign configuration values to class members
        lineRate = static_cast<uint64_t>(configuration.LineRate);
        captureSize = static_cast<uint64_t>(configuration.CaptureSizeMs);
        minNumOfIFGsPerPacket = configuration.MinNumOfIFGsPerPacket;

        // Convert configuration addresses and size to byte arrays
        destAddress = intToArray<uint64_t, 6>(configuration.DestAddress);
        sourceAddress = intToArray<uint64_t, 6>(configuration.SourceAddress);

        scs = static_cast<uint8_t>(configuration.SCS);
        maxNrb = fixRB(static_cast<uint16_t>(configuration.MaxNrb));
        nrbPerPacket = fixRB(static_cast<uint16_t>(configuration.NrbPerPacket));
        payloadType = configuration.PayloadType;

        // Important Calculations
        totalTransmisson = (lineRate * captureSize * 1000000) / 8;

        totalFrames = static_cast<double>(captureSize / FRAME_PERIOD_MS);
        packetsPERsymbol = static_cast<uint64_t>(ceil(static_cast<double>(maxNrb) / nrbPerPacket));
        packetsPERslot = static_cast<uint64_t>(packetsPERsymbol * SYMBOL_PER_SLOT);
        packetsPERsubframe = static_cast<uint64_t>(packetsPERslot * (scs / SCS_PERIODICITY));
        packetsPERframe = static_cast<uint64_t>(packetsPERsubframe * SUBFRAME_PER_FRAME);
        totalPackets = static_cast<uint64_t>(packetsPERframe * (captureSize / FRAME_PERIOD_MS) * totalFrames);

        iqSamplesPERpacket = static_cast<uint64_t>(2 * RE_PER_RB * nrbPerPacket);
        totalSamples = static_cast<uint64_t>(iqSamplesPERpacket * totalPackets);

        if (payloadType == "fixed")
        {
            iqSamples.insert(iqSamples.end(), configuration.iqSamples.begin(), configuration.iqSamples.end());
        }
        else if (payloadType == "random")
        {
            iqSamples.resize(totalSamples);

            // Random number generator setup
            random_device rd;  // Seed for the random number engine
            mt19937 gen(rd()); // Mersenne Twister random number engine
            uniform_int_distribution<> distrib(-128, 127);

            for (int8_t &byte : iqSamples)
            {
                byte = distrib(gen); // Generate a random number and assign it to the vector
            }
        }
        else
        {
            throw runtime_error("Wrong PayloadType");
        }
    }

    // Method to construct the full stream of packets and IFGs
    vector<uint8_t> generateStream()
    {
        cout << "========= Start Generating the Stream =========" << endl;

        uint8_t frameId{0};
        uint8_t subframeId{0};
        uint8_t slotId{0};
        uint8_t symbolId{0};
        uint16_t startPrbu{0};
        uint16_t ecpriSeqid{0};
        vector<int8_t> data;
        array<uint8_t, 2> etherSize;

        
        
        uint64_t sizeOfSamples = iqSamples.size();  // Size of the IQ samples vector 
        

        for (uint64_t packetNo = 0; packetNo < totalPackets; packetNo++)
        {
            
            // Loop to insert `nrbPerPacket` samples into `data` for the given `packetNo`
            data.clear();
            for (uint64_t i = 0; i < iqSamplesPERpacket; ++i)
            {
                // Calculate the index in the IQ sample vector using modulo for wrapping around
                int index = (packetNo * iqSamplesPERpacket + i) % sizeOfSamples;

                // Insert the IQ sample at this index into `data`
                data.push_back(iqSamples[index]);
            }


            OranPacket oranPacket{frameId, subframeId, slotId, symbolId, startPrbu, nrbPerPacket, data};
            auto ecpriPayload{oranPacket.getPacket()};

            EcpriPacket ecpriPacket{ecpriSeqid, ecpriPayload};
            auto etherPayload{ecpriPacket.getPacket()};

            etherSize = intToArray<uint64_t, 2>(size(etherPayload));

            EthernetPacket etherPacket{destAddress, sourceAddress, etherSize, etherPayload, minNumOfIFGsPerPacket};
            auto tempEtherPacket = etherPacket.getPacket();

            if (size(tempEtherPacket) > maxPacketSize)
            {
                throw runtime_error("Ethernet Frame exceeds the maximum allowed size");
            }

            fullPacket.insert(fullPacket.end(), tempEtherPacket.begin(), tempEtherPacket.end());
            
            if(packetNo)
            {
                if(packetNo%packetsPERframe == 0)
                    frameId = (frameId + 1)%255;
                if(packetNo%packetsPERsubframe == 0)
                    subframeId = (subframeId + 1)%10;
                if(packetNo%packetsPERslot == 0)
                    slotId = (slotId + 1)%((scs / SCS_PERIODICITY));
                if(packetNo%packetsPERsymbol == 0)
                    symbolId = (symbolId + 1)%14;
            }

            ecpriSeqid = (packetNo%255);
            startPrbu = startPrbu + nrbPerPacket;
        }

        IFGsNo = static_cast<int64_t>(totalTransmisson - size(fullPacket));
        silentIFGs.assign(IFGsNo, 0x07);

        if (IFGsNo < 0)
        {
            throw runtime_error("Negative IFGs");
        }

        fullPacket.insert(fullPacket.end(), silentIFGs.begin(), silentIFGs.end());

        cout << "Packets/Symbol: " << packetsPERsymbol << endl;
        cout << "Packets/Slot: " << packetsPERslot << endl;
        cout << "Packets/Subframe: " << packetsPERsubframe << endl;
        cout << "Packets/Frame: " << packetsPERframe << endl;
        cout << "IQ Samples/Packet: " << iqSamplesPERpacket << endl;
        cout << "Total Bytes: " << totalTransmisson << endl;
        cout << "Total Generated: " << size(fullPacket) << endl;
        cout << "Total Frames: " << totalFrames << endl;
        cout << "Total Packets: " << totalPackets << endl;
        cout << "Total IQ Samples: " << totalSamples << endl;
        cout << "Remaining IFGs: " << IFGsNo << endl;
        cout << "========= Done Generating the Stream =========" << endl;
        return fullPacket;
    }

    uint16_t fixRB(uint16_t RB)
    {
        if (RB == 0)
        {
            return uint16_t(273);
        }
        else
        {
            return RB;
        }
    }
};

void writePacketStreamToFile(const vector<uint8_t> &fullPacketStream, const string &fileName)
{
    // Create and open a text file to store the generated packet stream
    ofstream MyFile(fileName);

    // Counter to insert a line break after every 4 bytes
    uint8_t counter{0};

    cout << "========= Start Exporting the Stream =========" << endl;
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
    cout << "========= Done Exporting the Stream =========" << endl;

    // Close the file after writing is done
    MyFile.close();
}

//==================================================================================//
int main()
{
    // Vector to store the full packet stream generated later
    vector<uint8_t> fullPacketStream;

    // Parse configuration file "second_milestone.txt" to extract Ethernet settings
    parseConfigurations configuration("second_milestone.txt");

    // Create packetStreaming object using configuration data and payload
    packetStreaming packetStreaming{configuration};

    // Construct the full packet stream with bursts and IFGs
    fullPacketStream = packetStreaming.generateStream();

    // Call the new function to write the full packet stream to "packets.txt"
    writePacketStreamToFile(fullPacketStream, "packets.txt");

    return 0;
}
