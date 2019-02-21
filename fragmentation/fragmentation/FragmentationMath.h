/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2017 Semtech

Description: 	Firmware update over the air with LoRa proof of concept
				Functions for the decoding
*/

#ifndef _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAGMENTATION_MATH
#define _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAGMENTATION_MATH

#include "mbed.h"
#include "mbed_debug.h"
#include "FragBDWrapper.h"

#define FRAG_SESSION_ONGOING    0xffff

typedef struct
{
    int NbOfFrag;   // NbOfUtilFrames=SIZEOFFRAMETRANSMIT;
    int Redundancy; // nbr of extra frame
    int DataSize;   // included the lorawan specific data hdr,devadrr,...but without mic and payload decrypted
} FragmentationMathSessionParams_t;

// This file contains functions for the correction mechanisms designed by Semtech
class FragmentationMath
{
  public:
    /**
     * FragmentationMath
     * Initializes Semtech's library for Low-Density Parity Check Coding
     *
     * @param flash          Instance of wrapped BlockDevice
     * @param frame_count    Number of expected fragments (without redundancy packets)
     * @param frame_size     Size of a fragment (without LoRaWAN header)
     * @param redundancy_max Maximum number of redundancy packets
     */
    FragmentationMath(FragBDWrapper *flash, uint16_t frame_count, uint8_t frame_size, uint16_t redundancy_max, size_t flash_offset);

    ~FragmentationMath();

    /**
     * Initialize the FragmentationMath library. This function allocates the required buffers.
     *
     * @returns true if the memory was allocated, false if one or more allocations failed
     */
    bool initialize();

    /**
     * Let the library know that a frame was found.
     * @param frameCounter
     */
    void set_frame_found(uint16_t frameCounter);

    /**
     * Process a redundancy frame
     *
     * @param frameCounter      The frameCounter for this frame
     * @param rowData           Binary data of the frame (without LoRaWAN header)
     * @param sFotaParameter    Current state of the fragmentation session
     *
     * @returns FRAG_SESSION_ONGOING if the packets are not completed yet,
                any other value between 0..FRAG_SESSION_ONGOING if the packet was deconstructed
     */
    int process_redundant_frame(uint16_t frameCounter, uint8_t *rowData, FragmentationMathSessionParams_t sFotaParameter);

    /**
     * Get the number of lost frames
     */
    int get_lost_frame_count();

  private:
    void GetRowInFlash(int l, uint8_t *rowData);

    void StoreRowInFlash(uint8_t *rowData, int index);

    uint16_t FindMissingFrameIndex(uint16_t x);

    void FindMissingReceiveFrame(uint16_t frameCounter);

    /*!
    * \brief	Function to xor two line of data
    *
    * \param	[IN] dataL1 and dataL2
    * \param    [IN] size : number of Bytes in dataL1
    * \param	[OUT] xor(dataL1,dataL2) in dataL1
    */
    void XorLineData(uint8_t *dataL1, uint8_t *dataL2, int size);

    /*!
    * \brief	Function to xor two line of data
    *
    * \param	[IN] dataL1 and dataL2
    * \param    [IN] size : number of bool in dataL1
    * \param	[OUT] xor(dataL1,dataL2) store in dataL1
    */
    void XorLineBool(bool *dataL1, bool *dataL2, int size);

    /*!
    * \brief	Function to find the first one in a bolean vector
    *
    * \param	[IN] bool vector and size of vector
    * \param	[OUT] the position of the first one in the row vector
    */

    int FindFirstOne(bool *boolData, int size);

    /*!
    * \brief	Function to test if a vector is null
    *
    * \param	[IN] bool vector and size of vector
    * \param	[OUT] bool : true if vector is null
    */
    bool VectorIsNull(bool *boolData, int size);

    /*!
    * \brief	Function extact a row from the binary matrix and expand to a bool vector
    *
    * \param	[IN] row number
    * \param	[IN] bool vector, number of Bits in one row
    */
    void ExtractLineFromBinaryMatrix(bool *boolVector, int rownumber, int numberOfBit);

    /*!
    * \brief	Function Collapse and Push  a row vector to the binary matrix
    *
    * \param	[IN] row number
    * \param	[IN] bool vector, number of Bits in one row
    */
    void PushLineToBinaryMatrix(bool *boolVector, int rownumber, int numberOfBit);

    /*!
    * \brief	Function to calculate a certain row from the parity check matrix
    *
    * \param	[IN] i - the index of the row to be calculated
    * \param	[IN] M - the size of the row to be calculted, the number of uncoded fragments used in the scheme,matrixRow - pointer to the boolean array
    * \param	[OUT] void
    */
    void FragmentationGetParityMatrixRow(int N, int M, bool *matrixRow);

    /*!
    * \brief	Pseudo random number generator : prbs23
    * \param	[IN] x - the input of the prbs23 generator
    */
    int FragmentationPrbs23(int x);

    /*!
    * \brief	Function to determine whether a frame is a fragmentation command or fragmentation content
    *
    * \param	[IN]  input variable to be tested
    * \param	[OUT] return true if x is a power of two
    */
    bool IsPowerOfTwo(unsigned int x);

    FragBDWrapper *_flash;
    uint16_t _frame_count;
    uint8_t _frame_size;
    uint16_t _redundancy_max;
    size_t _flash_offset;

    uint8_t *matrixM2B;
    uint16_t *missingFrameIndex;

    bool *matrixRow;
    uint8_t *matrixDataTemp;
    bool *dataTempVector;
    bool *dataTempVector2;
    bool *s;
    uint8_t *xorRowDataTemp;

    int numberOfLoosingFrame;
    int lastReceiveFrameCnt;
};

#endif // _MBED_LORAWAN_UPDATE_CLIENT_CRYPTO_FRAGMENTATION_MATH
