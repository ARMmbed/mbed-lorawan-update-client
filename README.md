# Mbed OS LoRaWAN FUOTA Client

Firmware update over the Air library for LoRaWAN devices running Mbed OS 5. It implements the [LoRa Alliance Fragmented Data Block](https://lora-alliance.org/resource-hub/lorawan-fragmented-data-block-transport-specification-v100), [Remote Multicast Setup](https://lora-alliance.org/resource-hub/lorawan-remote-multicast-setup-specification-v100), and [Application Layer Clock Sync](https://lora-alliance.org/resource-hub/lorawan-application-layer-clock-synchronization-specification-v100) specifications. In addition it adds cryptographic verification of firmware, and supports delta updates. See [mbed-os-example-lorawan-fuota](https://github.com/armmbed/mbed-os-example-lorawan-fuota) for an example application and more documentation.

## Unit tests

Unit tests are in the `TESTS` folder and are ran with Mbed CLI. Note that you need to specify a storage layer (could be [HeapBlockDevice](https://os.mbed.com/docs/latest/apis/heapblockdevice.html) or [FlashIAPBlockDevice](https://os.mbed.com/docs/v5.10/apis/flashiapblockdevice.html)) in `TESTS/COMMON/test_setup.h`, and your storage configuration in `TESTS/tests/mbed_app.json`. Then run:

```
$ mbed test --app-config TESTS/tests/mbed_app.json -n mbed-lorawan-update-client-tests-tests-* -v
```

Omit `-v` for less verbose output.

## Memory usage

Most buffers are dynamically allocated when needed to save memory.

Memory usage is dependent on:

* The page size of the block device (one page needs to be allocated).

And during fragmentation on:

* Number of fragments required for a full file (without the redundancy packets) (`nbFrag`).
* The size of a data fragment (`fragSize`).
* The maximum number of redundancy frames that are expected (`nbRedundancy`).

Calculated via: `((nbRedundancy / 8) * nbRedundancy) + (nbFrag * 2) + (nbFrag) + (fragSize * 2) + (nbRedundancy * 3)`.

Use `printHeapStats()` to get an idea of the memory load.

For the L-TEK FF1705, with 528 bytes page size, a 7.844 byte image, 204 byte packets, and max. 40 redundancy packets:

* After calling the constructor: 432 bytes.
* During multicast setup: 432 bytes.
* During fragmentation: 1837 bytes (528 bytes page size, and 877 bytes for fragmentation buffers).
* After fragmentation: 960 bytes (only page size buffer active).
* During ECDSA-SHA256 verification: 5420 bytes.
* After ECDSA-SHA256 verification: 960 bytes.
* After calling the destructor: 0 bytes.

Note that these numbers do not include Mbed OS or the LoRaWAN stack. See [mbed-os-example-lorawan-fuota](https://github.com/armmbed/mbed-os-example-lorawan-fuota) for more information on total memory usage, and ways to optimize memory.

### Memory pressure events

Highest memory usage is seen when ECDSA-SHA256 verification is happening. To get the update client running on smaller boards you might want to clear out buffers in your application when this happens. There are two callbacks that fire when the verification starts and stops:

* `verificationStarting`.
* `verificationFinished`.
