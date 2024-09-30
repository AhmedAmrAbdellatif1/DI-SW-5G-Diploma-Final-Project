# ORAN and eCPRI Ethernet Packet Generation

This project is designed to generate and stream ORAN and eCPRI packets encapsulated within Ethernet frames. The code reads a configuration file, simulates IQ samples for ORAN packets, encapsulates them into eCPRI packets, and then wraps everything into Ethernet frames. The complete packet stream is written to a file in hexadecimal format, which can be used for testing and simulation purposes in ORAN and 5G systems.
## Structure

### Classes

- **`parseConfigurations`**: Parses the Ethernet and ORAN configuration from a text file, handling both fixed and random IQ sample generation.
- **`OranPacket`**: Constructs ORAN packets with headers and IQ samples.
- **`EcpriPacket`**: Encapsulates ORAN packets into eCPRI packets, generating the necessary headers.
- **`EthernetPacket`**: Encapsulates eCPRI packets within Ethernet frames, handling the headers, preamble, and CRC (Frame Check Sequence).
- **`packetStreaming`**: Generates the full stream of packets, including bursts, IFGs, and payloads. Manages packet stream construction and configuration-based calculations.

### Functions

- **`intToArray`**: Converts a number into an array of bytes with the least significant byte at the lowest index.
- **`convertIntoInteger`**: Converts a string into an unsigned integer, supporting both decimal and hexadecimal formats.
- **`writePacketStreamToFile`**: Writes the generated packet stream to a text file in hexadecimal format.

### Constants

- **`ETH_HEADER_SIZE`**: Defines the size of the Ethernet header.
- **`FRAME_PERIOD_MS`**: The duration of a frame period in milliseconds.
- **`SCS_PERIODICITY`**: Subcarrier spacing periodicity.
- **`SUBFRAME_PER_FRAME`**: Number of subframes per frame.
- **`SYMBOL_PER_SLOT`**: Number of symbols per slot.
- **`RE_PER_RB`**: Resource Elements per Resource Block.


## Output Format

The output file (`packets.txt`) contains the complete packet stream in hexadecimal format. Each packet consists of the following components:

- **Ethernet Frame**: The highest-level encapsulation, which includes:
  - Preamble and Start Frame Delimiter (SFD)
  - Source and Destination MAC addresses
  - EtherType/Size field
  - Frame Check Sequence (FCS)
  
- **eCPRI Packet**: Encapsulated within the Ethernet frame and contains:
  - eCPRI Header with version, message type, payload size, sequence ID, etc.
  - Payload (which is the ORAN packet)
  
- **ORAN Packet**: Encapsulated within the eCPRI packet and contains:
  - ORAN Packet Header, including frame ID, subframe ID, slot ID, symbol ID, section ID, etc.
  - IQ Samples, representing the payload data for the ORAN packet
  
The output is formatted as hexadecimal, with 4 bytes per line to improve readability.
, and CRC-32 validation.




