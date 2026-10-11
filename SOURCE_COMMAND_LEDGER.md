# Source command call-site ledger

**Branch:** `diagnostic-input-baseline`  
**Files included:** six core source/configuration files covered by [SOURCE_AUDIT.md](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/SOURCE_AUDIT.md).  
**Matching call sites:** 1434 lines containing command/transport/configuration operations.

This is a source-call-site ledger, not a normalized protocol transcript. It preserves source order within each file and leaves conditions, loops, mode branches, and function call expansion intact. It intentionally includes helper calls as well as raw I/O, because helpers such as `mailWrite`, `sparam`, or `configureCommonBlockB1` expand into multiple USB transactions. Expected response comments remain visible inline in the source call text when present. Use this together with the transport definitions and uncertainty notes in SOURCE_AUDIT.md.


## src/gchd/commands.cpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/commands.cpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 16 | `read_config_buffer` | `void GCHD::read_config_buffer( uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *buffer, uint16_t readSize) {` |
| 28 | `read_config` | `void GCHD::read_config(uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength) {` |
| 40 | `write_config_buffer` | `void GCHD::write_config_buffer( uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *buffer, uint16_t wLength) {` |
| 51 | `interruptPend` | `void GCHD::interruptPend()` |
| 83 | `doEnable` | `void GCHD::doEnable( uint16_t setMask, uint16_t valueMask ) //Set mask` |
| 109 | `enableAnalogInput` | `doEnable( EB_ANALOG_INPUT, analogInputBit );` |
| 113 | `clearEnableStateBits` | `void GCHD::clearEnableStateBits( uint16_t clearMask ) {` |
| 120 | `clearEnableStateBits` | `* and modes. scmd() is the short form for system command.` |
| 166 | `scmd` | `void GCHD::scmd(uint8_t command, uint8_t mode, uint16_t data) {` |
| 177 | `scmd` | `write_config_buffer(SCMD_REGISTER, send, 6);` |
| 181 | `scmd` | `write_config_buffer(SCMD_REGISTER, send+2, 4);` |
| 187 | `scmd` | `interruptPend();` |
| 198 | `completeStateChange` | `uint16_t GCHD::completeStateChange( uint16_t currentState,` |
| 255 | `stateConfirmedScmd` | `void GCHD::stateConfirmedScmd(uint8_t command,` |
| 283 | `stateConfirmedScmd` | `scmd(command, mode, data);` |
| 285 | `stateConfirmedScmd` | `currentState=completeStateChange( currentState, expectedState);` |
| 291 | `stateConfirmedScmd` | `void GCHD::stateConfirmedScmd(uint8_t command,` |
| 296 | `stateConfirmedScmd` | `stateConfirmedScmd( command, mode, data, currentState );` |
| 313 | `stateConfirmedScmd` | `*     sparam( address, lsb, bitLength, data );` |
| 321 | `sparam` | `void GCHD::sparam(uint16_t address, uint8_t lsb, uint8_t bits, uint16_t data) {` |
| 361 | `sparam` | `void GCHD::sparam(const bitfield_t &bitfield, uint16_t data) {` |
| 362 | `sparam` | `sparam(bitfield.address,` |
| 376 | `slsi` | `void GCHD::slsi(uint16_t address, uint16_t data) {` |
| 398 | `transcoderTableWrite` | `void GCHD::transcoderTableWrite(uint16_t address, std::vector<uint8_t> &data)` |
| 404 | `transcoderTableWrite` | `slsi( address, data );` |
| 411 | `transcoderTableWrite` | `slsi( address, data<<8 );` |
| 427 | `mailWrite` | `void GCHD::mailWrite( uint8_t port, const std::vector<unsigned char> &writeVector )` |
| 431 | `mailWrite` | `write_config_buffer( HD_MAIL_REGISTER, port << 8, (unsigned char *)writeVector.data(), writeVector.size() );` |
| 444 | `mailWrite` | `write_config_buffer( HDNEW_MAIL_WRITE, paddedBuffer.data(), paddedBuffer.size() );` |
| 450 | `mailWrite` | `write_config_buffer( HDNEW_MAIL_REQUEST_CONFIGURE, mailRequestBuffer, 4 );` |
| 451 | `mailWrite` | `interruptPend();` |
| 453 | `mailWrite` | `read_config( HDNEW_INTERRUPT_STATUS, 2 ); //EXPECTED=0x09, 0x00` |
| 457 | `mailWrite` | `read_config( HDNEW_MAIL_READ, 2 ); //Read to nowhere, dummy bytes.` |
| 476 | `mailRead` | `std::vector<unsigned char> GCHD::mailRead( uint8_t port,` |
| 483 | `mailRead` | `read_config_buffer( HD_MAIL_REGISTER, port<<8, input.data(), size );` |
| 490 | `mailRead` | `write_config_buffer( HDNEW_MAIL_REQUEST_CONFIGURE, mailRequestBuffer, 4 );` |
| 492 | `mailRead` | `interruptPend();` |
| 494 | `mailRead` | `read_config( HDNEW_INTERRUPT_STATUS, 2 ); //EXPECTED=0x09, 0x00` |
| 502 | `mailRead` | `read_config_buffer( HDNEW_MAIL_READ, input.data(), readSize ); //Read to nowhere, dummy bytes.` |
| 522 | `dlfirm` | `void GCHD::dlfirm(const char *file) {` |
| 555 | `readDevice0x9DCD` | `uint8_t GCHD::readDevice0x9DCD( uint8_t index )` |
| 559 | `readDevice0x9DCD` | `mailWrite( 0x33, input );` |
| 560 | `readDevice0x9DCD` | `return mailRead( 0x33, 1 )[0];` |
| 564 | `pollOn0x9989ED` | `void GCHD::pollOn0x9989ED()` |
| 569 | `pollOn0x9989ED` | `mailWrite( 0x33, VC{ 0x99, 0x89, 0xed } );` |
| 570 | `pollOn0x9989ED` | `input=mailRead( 0x33, 1 )[0];` |
| 575 | `readFrom0x9989EC` | `void GCHD::readFrom0x9989EC( unsigned count )` |
| 578 | `readFrom0x9989EC` | `mailWrite( 0x33, VC{ 0x99, 0x89, 0xec } );` |
| 579 | `readFrom0x9989EC` | `mailRead( 0x33, 1 ); //Currently don't put result anywhere.` |
| 590 | `stopStream` | `scmd(SCMD_STATE_CHANGE, 0x00, SCMD_STATE_NULL);` |
| 607 | `stopStream` | `completeStateChange(  SCMD_STATE_START, SCMD_STATE_NULL, emptyBuffer );` |
| 615 | `stopStream` | `scmd(SCMD_STATE_CHANGE, 0x00, SCMD_STATE_STOP);` |
| 616 | `stopStream` | `completeStateChange(  SCMD_STATE_NULL, SCMD_STATE_STOP, emptyBuffer );` |
| 627 | `clearEnableState` | `clearEnableStateBits(EB_ENCODER_ENABLE);` |
| 628 | `clearEnableState` | `clearEnableStateBits(EB_ENCODER_TRIGGER);` |
| 629 | `clearEnableState` | `clearEnableStateBits(EB_COMPOSITE_MUX);` |
| 630 | `clearEnableState` | `clearEnableStateBits(EB_ANALOG_MUX);` |
| 635 | `clearEnableState` | `mailWrite( 0x33, VC{0xab, 0xa9, 0x0f, 0xa4, 0x55} );` |
| 636 | `clearEnableState` | `std::vector<unsigned char> read=mailRead( 0x33, 3 ); //EXPECTED {0x27, 0xf9, 0x7b};` |
| 638 | `clearEnableState` | `doEnable(EB_FIRMWARE_PROCESSOR, 0x0); //Shut down processor.` |
| 645 | `clearEnableState` | `mailWrite( 0x33, VC{0xab, 0xa9, 0x0f, 0xa4, 0x55} );` |
| 646 | `clearEnableState` | `read=mailRead( 0x33, 3 ); //EXPECTED {0x33, 0x44, 0x55};` |
| 662 | `readVersion` | `read_config_buffer(HD_VERSION_REGISTER0, version.data(), 4);` |
| 663 | `readVersion` | `read_config_buffer(HD_VERSION_REGISTER1, version.data()+4, 4);` |
| 664 | `readVersion` | `read_config_buffer(HD_VERSION_REGISTER2, temporary, 4);` |
| 669 | `readVersion` | `read_config_buffer(HDNEW_VERSION_REGISTER0, version.data(), 4);` |
| 670 | `readVersion` | `read_config_buffer(HDNEW_VERSION_REGISTER1, version.data()+4, 4);` |
| 671 | `readVersion` | `read_config_buffer(HDNEW_VERSION_REGISTER2, temporary, 4);` |

## src/gchd/configure.cpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure.cpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 16 | `configureDevice` | `void GCHD::configureDevice()` |
| 42 | `configureDevice` | `state=completeStateChange(0x0000, 0x0000);` |
| 55 | `configureDevice` | `interruptPend();` |
| 69 | `configureDevice` | `dlfirm(firmwareIdle_.c_str());` |
| 80 | `configureDevice` | `read_config(0xbc, 0x0000, 0x0010, 2); //EXPECTED=0x20, 0x13` |
| 81 | `configureDevice` | `read_config(0xbc, 0x0000, 0x0012, 2); //EXPECTED=0x12, 0x10` |
| 82 | `configureDevice` | `read_config(0xbc, 0x0000, 0x0014, 2); //EXPECTED=0x18, 0x80` |
| 83 | `configureDevice` | `read_config(0xbc, 0x0000, 0x0016, 2); //EXPECTED=0x20, 0x30` |
| 84 | `configureDevice` | `read_config(0xbc, 0x0000, 0x0018, 2); //EXPECTED=0x20, 0x13` |
| 85 | `configureDevice` | `read_config(0xbc, 0x0000, 0x001a, 2); //EXPECTED=0x12, 0x10` |
| 86 | `configureDevice` | `read_config(0xbc, 0x0000, 0x001c, 2); //EXPECTED=0x18, 0x80` |
| 87 | `configureDevice` | `read_config(0xbc, 0x0000, 0x001e, 2); //EXPECTED=0x20, 0x30` |
| 94 | `configureDevice` | `stateConfirmedScmd( SCMD_RESET, 0x00, 0x0000 );` |
| 96 | `configureDevice` | `stateConfirmedScmd( SCMD_IDLE, 0x00, 0x0000 );` |
| 102 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0xa9, 0x0f, 0xa4, 0x55} );` |
| 103 | `configureDevice` | `mailRead( 0x33, 3 ); //EXPECTED {0x33, 0x44, 0x55}` |
| 113 | `configureDevice` | `mailWrite( 0x33, {0xab, 0xa9, 0x0f, 0xa4, 0x55} );` |
| 114 | `configureDevice` | `input=mailRead( 0x33, 3 ); /* read 3 bytes from 0x33 */` |
| 129 | `configureDevice` | `doEnable( EB_FIRMWARE_PROCESSOR, EB_FIRMWARE_PROCESSOR );` |
| 208 | `configureDevice` | `transcoderDefaultsInitialize();         //just came back from here.` |
| 209 | `configureDevice` | `scmd(SCMD_INIT, 0x00, 0x0000);` |
| 211 | `configureDevice` | `dlfirm(firmwareEnc_.c_str());` |
| 230 | `configureDevice` | `mailWrite( 0x33, {0xab, 0xa9, 0x0f, 0xa4, 0x5b} );` |
| 231 | `configureDevice` | `input=mailRead( 0x33, 3 ); /* read 3 bytes from 0x33 */` |
| 244 | `configureDevice` | `doEnable( EB_ENCODER_ENABLE, EB_ENCODER_ENABLE );` |
| 250 | `configureDevice` | `mailWrite( 0x33, {0xab, 0xa9, 0x0f, 0xa4, 0x55} );` |
| 251 | `configureDevice` | `input=mailRead( 0x33, 3 ); /* read 3 bytes from 0x33 */` |
| 256 | `configureDevice` | `mailWrite( 0x33, VC{0x28, 0x28} );` |
| 257 | `configureDevice` | `mailWrite( 0x33, VC{0x29, 0x89, 0x5b} );` |
| 258 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x91}` |
| 259 | `configureDevice` | `mailWrite( 0x33, VC{0xdd, 0xce, 0x3f, 0xb2} );` |
| 260 | `configureDevice` | `mailRead( 0x33, 2 ); //EXPECTED {0xda, 0x67}` |
| 262 | `configureDevice` | `doEnable( EB_ENCODER_TRIGGER, EB_ENCODER_TRIGGER ); //Turn on trigger bit` |
| 265 | `configureDevice` | `mailWrite( 0x33, VC{0x43, 0x23, 0x84} );` |
| 266 | `configureDevice` | `input=mailRead( 0x33, 1 ); //EXPECTED {0xf7}` |
| 268 | `configureDevice` | `doEnable( EB_ENCODER_TRIGGER, 0 ); //Turn off trigger bit` |
| 270 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfb} );` |
| 271 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 275 | `configureDevice` | `mailWrite( 0x44, VC{0x02, 0xc9} );` |
| 276 | `configureDevice` | `mailWrite( 0x44, VC{0x14, 0xd2} );` |
| 277 | `configureDevice` | `mailWrite( 0x44, VC{0x3c, 0x6b} );` |
| 278 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfa} );` |
| 279 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xed}` |
| 281 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xca} );` |
| 282 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xee}` |
| 283 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xe7} );` |
| 284 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x49}` |
| 285 | `configureDevice` | `mailWrite( 0x44, VC{0x03, 0x2a} );` |
| 286 | `configureDevice` | `mailWrite( 0x44, VC{0x05, 0x89} );` |
| 295 | `configureDevice` | `mailWrite( 0x44, VC{0x08, 0x91} );` |
| 296 | `configureDevice` | `mailWrite( 0x44, VC{0x09, 0xa8} );` |
| 298 | `configureDevice` | `mailWrite( 0x44, VC{0x08, 0x9b} );` |
| 299 | `configureDevice` | `mailWrite( 0x44, VC{0x09, 0x7a} );` |
| 301 | `configureDevice` | `mailWrite( 0x44, VC{0x19, 0xde} );` |
| 302 | `configureDevice` | `mailWrite( 0x44, VC{0x1a, 0x87} );` |
| 303 | `configureDevice` | `mailWrite( 0x44, VC{0x1b, 0x88} );` |
| 304 | `configureDevice` | `mailWrite( 0x44, VC{0x29, 0x8b} );` |
| 305 | `configureDevice` | `mailWrite( 0x44, VC{0x2d, 0x8f} );` |
| 306 | `configureDevice` | `mailWrite( 0x44, VC{0x4c, 0x89} );` |
| 307 | `configureDevice` | `mailWrite( 0x44, VC{0x55, 0x88} );` |
| 308 | `configureDevice` | `mailWrite( 0x44, VC{0x6b, 0xae} );` |
| 309 | `configureDevice` | `mailWrite( 0x44, VC{0x6c, 0xbe} );` |
| 310 | `configureDevice` | `mailWrite( 0x44, VC{0x6d, 0x78} );` |
| 311 | `configureDevice` | `mailWrite( 0x44, VC{0x6e, 0xa0} );` |
| 312 | `configureDevice` | `mailWrite( 0x44, VC{0x06, 0x08} );` |
| 316 | `configureDevice` | `mailWrite( 0x44, VC{0x02, 0xc9} );` |
| 317 | `configureDevice` | `mailWrite( 0x44, VC{0x14, 0xd2} );` |
| 318 | `configureDevice` | `mailWrite( 0x44, VC{0x3c, 0x6b} );` |
| 319 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfa} );` |
| 320 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xfd}` |
| 322 | `configureDevice` | `mailWrite( 0x44, VC{0x28, 0x88} );` |
| 323 | `configureDevice` | `mailWrite( 0x44, VC{0x10, 0x88} );` |
| 324 | `configureDevice` | `mailWrite( 0x44, VC{0x11, 0xd4} );` |
| 325 | `configureDevice` | `mailWrite( 0x44, VC{0x12, 0xd0} );` |
| 326 | `configureDevice` | `mailWrite( 0x44, VC{0x13, 0x08} );` |
| 327 | `configureDevice` | `mailWrite( 0x44, VC{0x14, 0x08} );` |
| 328 | `configureDevice` | `mailWrite( 0x44, VC{0x15, 0x88} );` |
| 329 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x47, 0xf9} );` |
| 330 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x40, 0xf3} );` |
| 331 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x43, 0xb7} );` |
| 332 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x4e, 0xb7} );` |
| 333 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x4f, 0xb7} );` |
| 334 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x48, 0xb7} );` |
| 335 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x49, 0xb7} );` |
| 336 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x58, 0x77} );` |
| 337 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x40, 0xf1} );` |
| 338 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x4d, 0xf5} );` |
| 339 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x4a, 0xaf} );` |
| 340 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x4b, 0xaf} );` |
| 341 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x5c, 0xb7} );` |
| 342 | `configureDevice` | `mailWrite( 0x33, VC{0x94, 0x46, 0xd7} );` |
| 344 | `configureDevice` | `readDevice0x9DCD(0x88); //EXPECTED 0xb2` |
| 345 | `configureDevice` | `mailWrite( 0x4e, VC{0xb7, 0xce} );` |
| 346 | `configureDevice` | `mailWrite( 0x4e, VC{0x41, 0xa3} );` |
| 347 | `configureDevice` | `mailWrite( 0x4e, VC{0xb8, 0xcc} );` |
| 348 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 349 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xcd} );` |
| 350 | `configureDevice` | `mailWrite( 0x4e, VC{0x0f, 0xce} );` |
| 351 | `configureDevice` | `mailWrite( 0x4e, VC{0x16, 0xfc} );` |
| 352 | `configureDevice` | `mailWrite( 0x4e, VC{0x17, 0xcc} );` |
| 353 | `configureDevice` | `mailWrite( 0x4e, VC{0x18, 0xcc} );` |
| 354 | `configureDevice` | `mailWrite( 0x4e, VC{0x19, 0xcc} );` |
| 355 | `configureDevice` | `mailWrite( 0x4e, VC{0x1a, 0x9c} );` |
| 356 | `configureDevice` | `readDevice0x9DCD(0x15); //EXPECTED 0xb2` |
| 357 | `configureDevice` | `mailWrite( 0x4e, VC{0x2a, 0xcb} );` |
| 358 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb3` |
| 359 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 360 | `configureDevice` | `mailWrite( 0x4e, VC{0x08, 0xcf} );` |
| 361 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 362 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xcd} );` |
| 365 | `configureDevice` | `mailWrite( 0x4e, VC{0x24, 0x8c} );` |
| 367 | `configureDevice` | `mailWrite( 0x4e, VC{0x24, 0x8d} );` |
| 369 | `configureDevice` | `mailWrite( 0x4e, VC{0x25, 0xcc} );` |
| 370 | `configureDevice` | `mailWrite( 0x4e, VC{0x30, 0x4c} );` |
| 371 | `configureDevice` | `mailWrite( 0x4e, VC{0x31, 0xcc} );` |
| 372 | `configureDevice` | `mailWrite( 0x4e, VC{0x32, 0xcc} );` |
| 373 | `configureDevice` | `mailWrite( 0x4e, VC{0x25, 0xcc} );` |
| 374 | `configureDevice` | `mailWrite( 0x4e, VC{0x26, 0xcc} );` |
| 375 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 376 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 377 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 378 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 379 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 380 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb3` |
| 381 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 382 | `configureDevice` | `mailWrite( 0x4e, VC{0xb0, 0xe8} );` |
| 383 | `configureDevice` | `readDevice0x9DCD(0x91); //EXPECTED 0xb2` |
| 384 | `configureDevice` | `mailWrite( 0x4e, VC{0xae, 0xc8} );` |
| 385 | `configureDevice` | `mailWrite( 0x4e, VC{0xb1, 0x0c} );` |
| 386 | `configureDevice` | `mailWrite( 0x4e, VC{0xb2, 0xcc} );` |
| 387 | `configureDevice` | `mailWrite( 0x4e, VC{0xb3, 0xcc} );` |
| 388 | `configureDevice` | `mailWrite( 0x4e, VC{0xb4, 0x99} );` |
| 389 | `configureDevice` | `readDevice0x9DCD(0x8b); //EXPECTED 0xe7` |
| 390 | `configureDevice` | `mailWrite( 0x4e, VC{0xb4, 0x98} );` |
| 391 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 392 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 393 | `configureDevice` | `mailWrite( 0x4e, VC{0x01, 0xad} );` |
| 394 | `configureDevice` | `mailWrite( 0x4e, VC{0x02, 0x39} );` |
| 395 | `configureDevice` | `readDevice0x9DCD(0x3c); //EXPECTED 0xb2` |
| 396 | `configureDevice` | `mailWrite( 0x4e, VC{0x03, 0xce} );` |
| 397 | `configureDevice` | `mailWrite( 0x4e, VC{0x04, 0xcd} );` |
| 398 | `configureDevice` | `mailWrite( 0x4e, VC{0x05, 0xcc} );` |
| 399 | `configureDevice` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 400 | `configureDevice` | `mailWrite( 0x4e, VC{0x1c, 0xd6} );` |
| 401 | `configureDevice` | `mailWrite( 0x4e, VC{0x1d, 0xcc} );` |
| 402 | `configureDevice` | `mailWrite( 0x4e, VC{0x1e, 0xcc} );` |
| 403 | `configureDevice` | `mailWrite( 0x4e, VC{0x1f, 0xcc} );` |
| 404 | `configureDevice` | `readDevice0x9DCD(0x1a); //EXPECTED 0xb2` |
| 405 | `configureDevice` | `mailWrite( 0x4e, VC{0x25, 0x6e} );` |
| 406 | `configureDevice` | `readDevice0x9DCD(0x3d); //EXPECTED 0x47` |
| 407 | `configureDevice` | `mailWrite( 0x4e, VC{0x02, 0x39} );` |
| 408 | `configureDevice` | `readDevice0x9DCD(0x38); //EXPECTED 0xb2` |
| 409 | `configureDevice` | `mailWrite( 0x4e, VC{0x07, 0xc8} );` |
| 410 | `configureDevice` | `mailWrite( 0x4e, VC{0x17, 0x0c} );` |
| 411 | `configureDevice` | `mailWrite( 0x4e, VC{0x19, 0x33} );` |
| 412 | `configureDevice` | `mailWrite( 0x4e, VC{0x1a, 0x33} );` |
| 413 | `configureDevice` | `mailWrite( 0x4e, VC{0x1b, 0x30} );` |
| 414 | `configureDevice` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 415 | `configureDevice` | `readDevice0x9DCD(0x1e); //EXPECTED 0xb2` |
| 416 | `configureDevice` | `mailWrite( 0x4e, VC{0x21, 0xcc} );` |
| 417 | `configureDevice` | `mailWrite( 0x4e, VC{0x22, 0xea} );` |
| 418 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 419 | `configureDevice` | `readDevice0x9DCD(0x11); //EXPECTED 0xb2` |
| 420 | `configureDevice` | `mailWrite( 0x4e, VC{0x2e, 0x6d} );` |
| 421 | `configureDevice` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfa} );` |
| 422 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xa4}` |
| 423 | `configureDevice` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf9} );` |
| 424 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x7f}` |
| 425 | `configureDevice` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf8} );` |
| 426 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x78}` |
| 427 | `configureDevice` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfe} );` |
| 428 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x0e}` |
| 429 | `configureDevice` | `mailWrite( 0x4c, VC{0x05, 0x88} );` |
| 430 | `configureDevice` | `mailWrite( 0x4c, VC{0x04, 0xb5} );` |
| 431 | `configureDevice` | `mailWrite( 0x4c, VC{0x04, 0x95} );` |
| 432 | `configureDevice` | `mailWrite( 0x4c, VC{0x61, 0xb8} );` |
| 433 | `configureDevice` | `mailWrite( 0x4c, VC{0x09, 0x3a} );` |
| 434 | `configureDevice` | `mailWrite( 0x4c, VC{0x0a, 0x70} );` |
| 435 | `configureDevice` | `mailWrite( 0x4c, VC{0x0b, 0xbf} );` |
| 436 | `configureDevice` | `mailWrite( 0x4c, VC{0xc9, 0x88} );` |
| 437 | `configureDevice` | `mailWrite( 0x4c, VC{0xca, 0x88} );` |
| 438 | `configureDevice` | `mailWrite( 0x4c, VC{0xcb, 0x88} );` |
| 439 | `configureDevice` | `mailWrite( 0x4c, VC{0xcc, 0x88} );` |
| 440 | `configureDevice` | `mailWrite( 0x4c, VC{0xcd, 0x88} );` |
| 441 | `configureDevice` | `mailWrite( 0x4c, VC{0xce, 0x88} );` |
| 442 | `configureDevice` | `mailWrite( 0x4c, VC{0xcf, 0x88} );` |
| 443 | `configureDevice` | `mailWrite( 0x4c, VC{0xd0, 0x88} );` |
| 447 | `configureDevice` | `mailWrite( 0x33, VC{0x21, 0x01, 0x72} );` |
| 448 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xf4}` |
| 449 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x02, 0x63} );` |
| 450 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x03, 0x63} );` |
| 451 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x04, 0x77} );` |
| 452 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x05, 0x73} );` |
| 453 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x06, 0x73} );` |
| 454 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x07, 0x33} );` |
| 455 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x08, 0x31} );` |
| 456 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x09, 0x33} );` |
| 457 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0a, 0x57} );` |
| 458 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0b, 0x7b} );` |
| 459 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0c, 0xf7} );` |
| 460 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0d, 0xf7} );` |
| 461 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0e, 0x73} );` |
| 462 | `configureDevice` | `mailWrite( 0x33, VC{0x20, 0x0f, 0x73} );` |
| 464 | `configureDevice` | `mailWrite( 0x33, VC{0xaa, 0x8f, 0x3b} );` |
| 471 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x03, 0x76} );` |
| 472 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 473 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x3b, 0x76} );` |
| 474 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 475 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x73, 0x76} );` |
| 476 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 477 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xab, 0x76} );` |
| 478 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 479 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xe3, 0x76} );` |
| 480 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 481 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x1b, 0x77} );` |
| 482 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 483 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x53, 0x77} );` |
| 484 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 485 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x8b, 0x77} );` |
| 486 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 487 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xc3, 0x77} );` |
| 488 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 489 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xfb, 0x77} );` |
| 490 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 491 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x33, 0x74} );` |
| 492 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 493 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x6b, 0x74} );` |
| 494 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 495 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xa3, 0x74} );` |
| 496 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 497 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xdb, 0x74} );` |
| 498 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 499 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x13, 0x75} );` |
| 500 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 501 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x4b, 0x75} );` |
| 502 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 503 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x83, 0x75} );` |
| 504 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 505 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xbb, 0x75} );` |
| 506 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 507 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xf3, 0x75} );` |
| 508 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 509 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x2b, 0x72} );` |
| 510 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 511 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x63, 0x72} );` |
| 512 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 513 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x9b, 0x72} );` |
| 514 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 515 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xd3, 0x72} );` |
| 516 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 517 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x0b, 0x73} );` |
| 518 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 519 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x43, 0x73} );` |
| 520 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 521 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x7b, 0x73} );` |
| 522 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 523 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xb3, 0x73} );` |
| 524 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 525 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xeb, 0x73} );` |
| 526 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 527 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x23, 0x70} );` |
| 528 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 529 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x5b, 0x70} );` |
| 530 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 531 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x93, 0x70} );` |
| 532 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 533 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xcb, 0x70} );` |
| 534 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 535 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x03, 0x71} );` |
| 536 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 537 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x3b, 0x71} );` |
| 538 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 539 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x73, 0x71} );` |
| 540 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 541 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xab, 0x71} );` |
| 542 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 543 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xe3, 0x71} );` |
| 544 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 545 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x1b, 0x7e} );` |
| 546 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 547 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x53, 0x7e} );` |
| 548 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 549 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x8b, 0x7e} );` |
| 550 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 551 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xc3, 0x7e} );` |
| 552 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 553 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xfb, 0x7e} );` |
| 554 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 555 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x33, 0x7f} );` |
| 556 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 557 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x6b, 0x7f} );` |
| 558 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 559 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xa3, 0x7f} );` |
| 560 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 561 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xdb, 0x7f} );` |
| 562 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 563 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x13, 0x7c} );` |
| 564 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 565 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x4b, 0x7c} );` |
| 566 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 567 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x83, 0x7c} );` |
| 568 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 569 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xbb, 0x7c} );` |
| 570 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 571 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xf3, 0x7c} );` |
| 572 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 573 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x2b, 0x7d} );` |
| 574 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 575 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x63, 0x7d} );` |
| 576 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 577 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x9b, 0x7d} );` |
| 578 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 579 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xd3, 0x7d} );` |
| 580 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 581 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x0b, 0x7a} );` |
| 582 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 583 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x43, 0x7a} );` |
| 584 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 585 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x7b, 0x7a} );` |
| 586 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 587 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xb3, 0x7a} );` |
| 588 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 589 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xeb, 0x7a} );` |
| 590 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 591 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x23, 0x7b} );` |
| 592 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 593 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x5b, 0x7b} );` |
| 594 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 595 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x93, 0x7b} );` |
| 596 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 597 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xcb, 0x7b} );` |
| 598 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 599 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x03, 0x78} );` |
| 600 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 601 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x3b, 0x78} );` |
| 602 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 603 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x73, 0x78} );` |
| 604 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 605 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xab, 0x78} );` |
| 606 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 607 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xe3, 0x78} );` |
| 608 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 609 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x1b, 0x79} );` |
| 610 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 611 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x53, 0x79} );` |
| 612 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 613 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0x8b, 0x79} );` |
| 614 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 615 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0x92, 0x3e, 0xb4, 0xc3, 0x79} );` |
| 616 | `configureDevice` | `mailRead( 0x33, 56 );` |
| 617 | `configureDevice` | `mailWrite( 0x33, VC{0xab, 0xa2, 0x3e, 0xb4, 0xfb, 0x79} );` |
| 620 | `configureDevice` | `mailRead( 0x33, 8 ); //EXPECTED {0xe9, 0x5c, 0xcf, 0x42, 0xb5, 0x28, 0x9b, 0x0e}` |
| 621 | `configureDevice` | `mailWrite( 0x33, VC{0xaa, 0x8d, 0x35} );` |
| 623 | `configureDevice` | `mailWrite( 0x33, {0xab, 0xa9, 0x0f, 0xa4, 0x5b} );` |
| 624 | `configureDevice` | `input=mailRead( 0x33, 3 ); /* read 3 bytes from 0x33 */` |
| 639 | `configureDevice` | `transcoderSetup( currentInputSettings_, currentTranscoderSettings_ );` |
| 645 | `configureDevice` | `scmd(SCMD_INIT, 0xa0, 0x0000);` |
| 647 | `configureDevice` | `mailWrite( 0x44, VC{0x06, 0x86} );` |
| 649 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfd} );` |
| 650 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 652 | `configureDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xf8} );` |
| 653 | `configureDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xcc}` |
| 656 | `configureDevice` | `mailWrite( 0x44, VC{0x03, 0x2f} );` |
| 658 | `configureDevice` | `mailWrite( 0x44, VC{0x03, 0x28} );` |
| 660 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 661 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 663 | `configureDevice` | `mailWrite( 0x4e, VC{0xb3, 0xcc} );` |
| 665 | `configureDevice` | `mailWrite( 0x4e, VC{0xb3, 0x33} );` |
| 667 | `configureDevice` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 668 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 670 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0xcc} );` |
| 672 | `configureDevice` | `mailWrite( 0x4e, VC{0x27, 0x33} );` |
| 673 | `configureDevice` | `readDevice0x9DCD(0x3f);` |
| 674 | `configureDevice` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 675 | `configureDevice` | `readDevice0x9DCD(0x6e);` |
| 676 | `configureDevice` | `mailWrite( 0x4e, VC{0x51, 0xcc} );` |
| 678 | `configureDevice` | `doEnable( EB_COMPOSITE_MUX, composite ? EB_COMPOSITE_MUX: 0);` |
| 679 | `configureDevice` | `doEnable( EB_ANALOG_INPUT, analog ? EB_ANALOG_INPUT: 0);` |
| 680 | `configureDevice` | `doEnable( EB_ANALOG_MUX, analog ? EB_ANALOG_MUX:0);` |
| 686 | `configureDevice` | `configureHDMI();` |
| 689 | `configureDevice` | `configureComponent();` |
| 692 | `configureDevice` | `configureComposite();` |
| 714 | `uninitDevice` | `mailWrite( 0x44, VC{0x06, 0x86} );` |
| 715 | `uninitDevice` | `mailWrite( 0x33, VC{0x89, 0x89, 0xf8} );` |
| 716 | `uninitDevice` | `mailRead( 0x33, 1 ); //EXPECTED {0xc9}` |
| 717 | `uninitDevice` | `mailWrite( 0x44, VC{0x03, 0x2f} );` |
| 728 | `uninitDevice` | `scmd(SCMD_INIT, 0xa0, 0x0000);` |
| 734 | `uninitDevice` | `doEnable(EB_FIRMWARE_PROCESSOR, 0x0);` |
| 736 | `uninitDevice` | `stateConfirmedScmd( SCMD_IDLE, 0x00, 0x0000 );` |
| 737 | `uninitDevice` | `stateConfirmedScmd( SCMD_RESET, 0x01, 0x0000 );` |
| 741 | `configureColorSpace` | `void GCHD::configureColorSpace()` |
| 746 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x0b, 0x4c} );` |
| 747 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x0c, 0x4c} );` |
| 748 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x0d, 0x4c} );` |
| 749 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x08, 0x4c} );` |
| 750 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x09, 0x4c} );` |
| 751 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x0a, 0x4c} );` |
| 752 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x1e, 0x98} );` |
| 753 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x1f, 0xc9} );` |
| 758 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x92, 0xaa} );` |
| 759 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x93, 0xdc} );` |
| 760 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x94, 0xcc} );` |
| 761 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x95, 0xcc} );` |
| 762 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x96, 0xcc} );` |
| 763 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x97, 0xcc} );` |
| 764 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x98, 0xcc} );` |
| 765 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x99, 0xcc} );` |
| 766 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9a, 0xcc} );` |
| 767 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9b, 0xdc} );` |
| 768 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9c, 0xcc} );` |
| 769 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9d, 0xcc} );` |
| 770 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9e, 0xcc} );` |
| 771 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9f, 0xcc} );` |
| 772 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa0, 0xcc} );` |
| 773 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa1, 0xcc} );` |
| 774 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa2, 0xcc} );` |
| 775 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa3, 0xdc} );` |
| 776 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa4, 0xcc} );` |
| 777 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa5, 0xec} );` |
| 778 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa6, 0xcc} );` |
| 779 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa7, 0xc8} );` |
| 780 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa8, 0xcd} );` |
| 781 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa9, 0xec} );` |
| 782 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xaa, 0xcc} );` |
| 784 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x92, 0x8c} );` |
| 785 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x93, 0xcb} );` |
| 786 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x94, 0xca} );` |
| 787 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x95, 0xce} );` |
| 788 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x96, 0x22} );` |
| 789 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x97, 0xb2} );` |
| 790 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x98, 0xae} );` |
| 791 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x99, 0xb5} );` |
| 792 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9a, 0x52} );` |
| 793 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9b, 0xc5} );` |
| 794 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9c, 0x1f} );` |
| 795 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9d, 0xb6} );` |
| 796 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9e, 0x54} );` |
| 797 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x9f, 0xb3} );` |
| 798 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa0, 0x90} );` |
| 799 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa1, 0xcc} );` |
| 800 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa2, 0x32} );` |
| 801 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa3, 0xcb} );` |
| 802 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa4, 0xca} );` |
| 803 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa5, 0xec} );` |
| 804 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa6, 0xcc} );` |
| 805 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa7, 0xc8} );` |
| 806 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa8, 0xcd} );` |
| 807 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xa9, 0xec} );` |
| 808 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xaa, 0xcc} );` |
| 816 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb2, 0xcc} );` |
| 817 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb5, 0xc4} );` |
| 820 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x03, 0x0c} );` |
| 822 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x03, 0x04} );` |
| 826 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb2, 0xcc} );` |
| 827 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb5, 0xc4} );` |
| 828 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 832 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb2, 0xcf} );` |
| 833 | `configureColorSpace` | `mailWrite( 0x4e, VC{0xb5, 0xc4} );` |
| 836 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 838 | `configureColorSpace` | `mailWrite( 0x4e, VC{0x03, 0x84} );` |
| 848 | `configureSetupSubblock` | `void GCHD::configureSetupSubblock()` |
| 850 | `configureSetupSubblock` | `mailWrite( 0x33, VC{0x99, 0x89, 0x8b} );` |
| 851 | `configureSetupSubblock` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 856 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc8} );` |
| 857 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0x88} );` |
| 858 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0x77} );` |
| 859 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x92, 0x77} );` |
| 860 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x93, 0x77} );` |
| 861 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x94, 0x77} );` |
| 862 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0x77} );` |
| 863 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x77} );` |
| 864 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x77} );` |
| 865 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x98, 0x77} );` |
| 866 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x99, 0x77} );` |
| 867 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9a, 0x77} );` |
| 868 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9b, 0x77} );` |
| 869 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9c, 0x77} );` |
| 870 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9d, 0x77} );` |
| 871 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9e, 0x77} );` |
| 872 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x9f, 0x77} );` |
| 873 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x77} );` |
| 874 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0x77} );` |
| 875 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 876 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x77} );` |
| 886 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc0} );` |
| 887 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 888 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0xae} );` |
| 889 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0xc2} );` |
| 890 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0xde} );` |
| 891 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x0a} );` |
| 892 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 893 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8a} );` |
| 894 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0xf8} );` |
| 895 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0xbc} );` |
| 896 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x1a} );` |
| 898 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc0} );` |
| 899 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 900 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0x7e} );` |
| 901 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0x77} );` |
| 902 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0xde} );` |
| 903 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x0a} );` |
| 904 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 905 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8c} );` |
| 906 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0x18} );` |
| 907 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 908 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x77} );` |
| 912 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc0} );` |
| 913 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 914 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0xfe} );` |
| 915 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0xbb} );` |
| 916 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0xe4} );` |
| 917 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x1c} );` |
| 918 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 919 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8d} );` |
| 920 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0x28} );` |
| 921 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 922 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x77} );` |
| 926 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xd0} );` |
| 927 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 928 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0xd8} );` |
| 929 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0xbf} );` |
| 930 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0x9d} );` |
| 931 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x1b} );` |
| 932 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 933 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8a} );` |
| 934 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0xd8} );` |
| 935 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0xb2} );` |
| 936 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x59} );` |
| 938 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc0} );` |
| 939 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 940 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0x28} );` |
| 941 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0x93} );` |
| 942 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0x82} );` |
| 943 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0xc2} );` |
| 944 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 945 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8d} );` |
| 946 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0x28} );` |
| 947 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 948 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x77} );` |
| 953 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xd0} );` |
| 954 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 955 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0x68} );` |
| 956 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0xbf} );` |
| 957 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0xab} );` |
| 958 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0x17} );` |
| 959 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 960 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x8c} );` |
| 961 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0xf8} );` |
| 962 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x82} );` |
| 963 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x59} );` |
| 965 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x70, 0xc0} );` |
| 966 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 967 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x90, 0x78} );` |
| 968 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x91, 0xb9} );` |
| 969 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x95, 0x86} );` |
| 970 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x96, 0xc4} );` |
| 971 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0x97, 0x88} );` |
| 972 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa0, 0x81} );` |
| 973 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa1, 0x78} );` |
| 974 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 975 | `configureSetupSubblock` | `mailWrite( 0x4c, VC{0xa3, 0x77} );` |
| 990 | `configureCommonBlockA` | `void GCHD::configureCommonBlockA()` |
| 992 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 993 | `configureCommonBlockA` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 994 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 995 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 996 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 997 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 998 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 999 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1000 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1001 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1002 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0x88} );` |
| 1003 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1004 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1005 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1006 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1007 | `configureCommonBlockA` | `readFrom0x9989EC(32);` |
| 1008 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1009 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1010 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1011 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1012 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1013 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0xa8} );` |
| 1014 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1015 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1016 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1017 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1018 | `configureCommonBlockA` | `readFrom0x9989EC(32);` |
| 1019 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1020 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1021 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1022 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1023 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1024 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0xc8} );` |
| 1025 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1026 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1027 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1028 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1029 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x2b, 0x6e, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0x70, 0x6f, 0x73, 0x6e, 0x1c, 0x3f, 0xbe, 0x70, 0x4e, 0x00, 0x46, 0x3b, 0x6e, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0x70, 0x6e, 0x6e, 0x6e, 0x93, 0x6e, 0x5c}` |
| 1030 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1031 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1032 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1033 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1034 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1035 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0xe8} );` |
| 1036 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1037 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1038 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1039 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1040 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x52, 0x70, 0x3f, 0x7f, 0x6e, 0x64, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x6e, 0x6e, 0x6e, 0x92, 0x6e, 0x3d, 0x17, 0x00, 0x0d, 0x23, 0x0f, 0x1d, 0x1a, 0x0b, 0x1c, 0x64, 0x4e, 0x4e, 0x6f, 0xfa}` |
| 1042 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 1043 | `configureCommonBlockA` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 1044 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1045 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1046 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1047 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1048 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1049 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1050 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1051 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1052 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0x08} );` |
| 1053 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1054 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1055 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1056 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1057 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x6c, 0x6d, 0x72, 0x9f, 0x26, 0xfe, 0x6a, 0x71, 0x6b, 0x7a, 0x7d, 0x6d, 0x7c, 0x4d, 0x67, 0x69, 0x69, 0xed, 0x6f, 0x6e, 0x6e, 0x08, 0x6d, 0x62, 0x6e, 0x7e, 0x6e, 0xee, 0x6f, 0x73, 0xee, 0xbe}` |
| 1058 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1059 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1060 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1061 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1062 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0x28} );` |
| 1063 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1064 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1065 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1066 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1067 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x1c, 0x72, 0x78, 0x4e, 0x7e, 0x42, 0x4b, 0xee, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0xf0, 0x6f, 0x73, 0xee, 0x76, 0x1f, 0x72, 0x78, 0x4e, 0x36, 0x42, 0x4b, 0x6e, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0xf0}` |
| 1068 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1069 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1070 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1071 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1072 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0x48} );` |
| 1073 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1074 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1075 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1076 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1077 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x6f, 0x73, 0x6e, 0xd2, 0x3c, 0xbe, 0x70, 0x4e, 0xd6, 0x46, 0x3b, 0x2e, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0x70, 0xe2, 0x64, 0xbe, 0xfe, 0x4e, 0x2e, 0x5f, 0x4e, 0x62, 0x2e, 0x3b, 0x6e, 0xce, 0x34}` |
| 1078 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x81} );` |
| 1079 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x65, 0x8a} );` |
| 1080 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1081 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0x28} );` |
| 1082 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0x68} );` |
| 1083 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0xa8} );` |
| 1084 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x14, 0x88} );` |
| 1085 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x8b} );` |
| 1086 | `configureCommonBlockA` | `pollOn0x9989ED();` |
| 1087 | `configureCommonBlockA` | `readFrom0x9989EC(32); //EXPECTED {0x6e, 0x6e, 0x6e, 0x76, 0xe2, 0x64, 0xbe, 0xe4, 0x4e, 0x8e, 0x43, 0x7e, 0x7e, 0x50, 0xf8, 0x6e, 0xce, 0x34, 0x6e, 0x6e, 0x6e, 0x76, 0x6e, 0x6e, 0x6e, 0x6e, 0x6e, 0x6e, 0x6e, 0x6e, 0x6e, 0x28}` |
| 1090 | `configureCommonBlockA` | `configureCommonBlockC(); //Think this is just a driver choice to move it to just doing it at the end on newer model` |
| 1092 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1093 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x10, 0x89} );` |
| 1094 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x11, 0xfc} );` |
| 1095 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x12, 0xc8} );` |
| 1096 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x13, 0x8b} );` |
| 1097 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x15, 0x88} );` |
| 1098 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xed} );` |
| 1099 | `configureCommonBlockA` | `mailRead( 0x33, 1 ); //EXPECTED {0xec}` |
| 1100 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x8a, 0xbf} );` |
| 1101 | `configureCommonBlockA` | `mailRead( 0x33, 2 ); //EXPECTED {0x6e, 0xe1}` |
| 1102 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xb8} );` |
| 1103 | `configureCommonBlockA` | `std::vector<uint8_t> readValue=mailRead( 0x33, 1 );` |
| 1105 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 1106 | `configureCommonBlockA` | `mailRead( 0x33, 1 ); //EXPECTED {0x02}` |
| 1108 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 1109 | `configureCommonBlockA` | `mailRead( 0x33, 1 ); //EXPECTED {0x02}` |
| 1110 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x10, 0x00, 0x33} );` |
| 1114 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x10, 0x01, setValue} );` |
| 1115 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x10, 0x02, 0x71} );` |
| 1116 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x10, 0x03, 0x72} );` |
| 1121 | `configureCommonBlockA` | `mailWrite( 0x33, VC{0x10, 0x01, setValue} );` |
| 1124 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x04, 0x95} );` |
| 1127 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1128 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0xc1, 0x89} );` |
| 1129 | `configureCommonBlockA` | `mailWrite( 0x4c, VC{0xc6, 0x8b} );` |
| 1134 | `configureCommonBlockB1` | `void GCHD::configureCommonBlockB1( bool mysteryParameter )` |
| 1136 | `configureCommonBlockB1` | `mailWrite( 0x33, VC{0x99, 0x89, 0x89} );` |
| 1137 | `configureCommonBlockB1` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 1138 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x72, 0x88} );` |
| 1139 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0xc0, 0x89} );` |
| 1140 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x61, 0x98} );` |
| 1141 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x63, 0x89} );` |
| 1145 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x64, 0xde} );` |
| 1147 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x64, 0x96} );` |
| 1150 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x65, 0x88} );` |
| 1151 | `configureCommonBlockB1` | `mailWrite( 0x33, VC{0x99, 0x89, 0xff} );` |
| 1152 | `configureCommonBlockB1` | `mailRead( 0x33, 1 ); //EXPECTED {0x73}` |
| 1153 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x04, 0x8d} );` |
| 1157 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x62, 0x01} );` |
| 1159 | `configureCommonBlockB1` | `mailWrite( 0x4c, VC{0x62, 0x91} );` |
| 1163 | `configureCommonBlockB2` | `void GCHD::configureCommonBlockB2()` |
| 1165 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x89} );` |
| 1166 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x58, 0xd8} );` |
| 1172 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x59, 0xd0} );` |
| 1175 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x59, 0x20} );` |
| 1178 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5a, 0x88} );` |
| 1185 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x8d} );` |
| 1187 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x98} );` |
| 1189 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x88} );` |
| 1192 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x8c} );` |
| 1193 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x88} );` |
| 1197 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x9d} );` |
| 1198 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x89} );` |
| 1200 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x99} );` |
| 1201 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x88} );` |
| 1206 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x8e} );` |
| 1207 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x89} );` |
| 1209 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5b, 0x8a} );` |
| 1210 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5c, 0x88} );` |
| 1218 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5e, 0x88} );` |
| 1219 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5f, 0x88} );` |
| 1220 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x60, 0x88} );` |
| 1221 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x61, 0x88} );` |
| 1222 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x62, 0x88} );` |
| 1223 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x63, 0x88} );` |
| 1224 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x64, 0x88} );` |
| 1225 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x65, 0x88} );` |
| 1258 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x5d, value} );` |
| 1259 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1260 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xcd, 0x8b} );` |
| 1261 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xff} );` |
| 1262 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6b}` |
| 1263 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x04, 0x8d} );` |
| 1272 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 1273 | `configureCommonBlockB2` | `testValue = mailRead( 0x33, 1 )[0] & 0x90;` |
| 1276 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 1277 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 1278 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfc} );` |
| 1279 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 1280 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf3} );` |
| 1281 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x7e}` |
| 1282 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 1283 | `configureCommonBlockB2` | `status=mailRead( 0x33, 1 )[0];` |
| 1285 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x65)} );` |
| 1286 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x64)} );` |
| 1287 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0d, 0xc8} );` |
| 1290 | `configureCommonBlockB2` | `status=mailRead( 0x33, 1 )[0];` |
| 1292 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x65)} );` |
| 1293 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x64)} );` |
| 1294 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0d, 0xc8} ); //This comes before` |
| 1298 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1299 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xc1, 0x89} );` |
| 1300 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xc6, 0x8b} );` |
| 1301 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1302 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x61, 0x88} );` |
| 1304 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x10, 0x00, 0x33} );` |
| 1305 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x10, 0x01, 0xe3} );` |
| 1306 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x10, 0x02, 0x71} );` |
| 1307 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x10, 0x03, 0x72} );` |
| 1308 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x10, 0x01, 0x63} );` |
| 1309 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x89} );` |
| 1314 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x33, 0x08} );` |
| 1315 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x34, 0xa5} );` |
| 1317 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x33, 0x88} );` |
| 1318 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x34, 0x90} );` |
| 1322 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x33, 0x08} );` |
| 1323 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x34, 0xa5} );` |
| 1327 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x33, 0x88} );` |
| 1328 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x34, 0x90} );` |
| 1334 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x35, 0x88} );` |
| 1335 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1336 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xc5, 0x88} );` |
| 1337 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xa3} );` |
| 1338 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x7f}` |
| 1339 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x58, 0x9d} );` |
| 1340 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1341 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xff} );` |
| 1342 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6b}` |
| 1343 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x04, 0x8d} );` |
| 1344 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xe2, 0x6c} );` |
| 1345 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xe3, 0x88} );` |
| 1346 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xe4, 0x80} );` |
| 1347 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xe0, 0x49} );` |
| 1348 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xe1, 0x89} );` |
| 1349 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x89} );` |
| 1350 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x91, 0x88} );` |
| 1351 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x92, 0x88} );` |
| 1352 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x93, 0x89} );` |
| 1353 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x94, 0xa9} );` |
| 1354 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x95, 0xcb} );` |
| 1355 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x96, 0xed} );` |
| 1356 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x97, 0x0f} );` |
| 1357 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x98, 0x8a} );` |
| 1358 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x99, 0x53} );` |
| 1359 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1360 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xff} );` |
| 1361 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6b}` |
| 1362 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x04, 0x8c} );` |
| 1363 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x89} );` |
| 1364 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x68, 0x89} );` |
| 1365 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x6b, 0x88} );` |
| 1366 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x6c, 0x88} );` |
| 1367 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x6d, 0xf8} );` |
| 1368 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1369 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xce, 0x8b} );` |
| 1370 | `configureCommonBlockB2` | `mailWrite( 0x33, VC{0x99, 0x89, 0xff} );` |
| 1371 | `configureCommonBlockB2` | `mailRead( 0x33, 1 ); //EXPECTED {0x6a}` |
| 1372 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x04, 0x88} );` |
| 1373 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1374 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xc1, 0x88} );` |
| 1375 | `configureCommonBlockB2` | `mailWrite( 0x4c, VC{0xc6, 0x8b} );` |
| 1378 | `configureCommonBlockB3` | `void GCHD::configureCommonBlockB3()` |
| 1381 | `configureCommonBlockB3` | `mailWrite( 0x4c, VC{0x0f, 0x89} );` |
| 1382 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0x5b} );` |
| 1383 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x7c}` |
| 1384 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0x5a} );` |
| 1385 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x2a}` |
| 1386 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0x59} );` |
| 1387 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x6c}` |
| 1389 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0xc8} );` |
| 1390 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 1391 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0xcf} );` |
| 1392 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x76}` |
| 1393 | `configureCommonBlockB3` | `mailWrite( 0x33, VC{0x99, 0x89, 0xce} );` |
| 1394 | `configureCommonBlockB3` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e};` |
| 1395 | `configureCommonBlockB3` | `mailWrite( 0x4c, VC{0x0f, 0x88} );` |
| 1399 | `configureCommonBlockC` | `void GCHD::configureCommonBlockC()` |
| 1401 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xaa, 0xb8, 0x29, 0xf6} );` |
| 1402 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x73} );` |
| 1403 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0xa6, 0x33, 0xc0, 0x4d, 0xda, 0x67, 0x0b}` |
| 1404 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x7b} );` |
| 1405 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xf2, 0xb8, 0xcd, 0x3f, 0xb2, 0x25, 0x98, 0x0b}` |
| 1406 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x63} );` |
| 1407 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xf7, 0x4e, 0xcd, 0x3c, 0x32, 0x25, 0x9a, 0x73}` |
| 1408 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x6b} );` |
| 1409 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xfc, 0x96, 0xb8, 0x9c, 0xe5, 0x69, 0x28, 0x28}` |
| 1410 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x53} );` |
| 1411 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xef, 0x11, 0x80, 0x3f, 0xb2, 0x25, 0x19, 0xcb}` |
| 1412 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x5b} );` |
| 1413 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x37, 0x99, 0xcd, 0x3e, 0xb3, 0x24, 0x99, 0x0a}` |
| 1414 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x43} );` |
| 1415 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe7, 0x58, 0xcd, 0x3e, 0xb3, 0x24, 0x9a, 0x31}` |
| 1416 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x4b} );` |
| 1417 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x66, 0x41, 0xbd, 0x07, 0x9f, 0x65, 0xc0, 0x27}` |
| 1418 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x33} );` |
| 1419 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xa3, 0x59, 0x08, 0xb1, 0x93, 0x25, 0x98, 0x15}` |
| 1420 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x3b} );` |
| 1421 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0x59, 0xcc, 0x27, 0xb2, 0x39, 0x8e, 0x2b}` |
| 1422 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x23} );` |
| 1423 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xbe, 0x75, 0xe9, 0x3f, 0x76, 0xab, 0xb9, 0x0b}` |
| 1424 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x2b} );` |
| 1425 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0xc7, 0xcc, 0x3f, 0xb2, 0xd9, 0x98, 0x4e}` |
| 1426 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x13} );` |
| 1427 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x8a, 0x3e, 0xad, 0x4b, 0xdd, 0x2f, 0xb8, 0x2b}` |
| 1428 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x1b} );` |
| 1429 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xc6, 0x79, 0xec, 0x1f, 0xb2, 0x25, 0x98, 0xf6}` |
| 1430 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x03} );` |
| 1431 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0x4e, 0xf1, 0x26, 0xf4, 0x2a, 0x98, 0x01}` |
| 1432 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x0b} );` |
| 1433 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xc6, 0x79, 0xec, 0x1f, 0x92, 0x05, 0x99, 0xee}` |
| 1434 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xf3} );` |
| 1435 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe4, 0x5a, 0xea, 0xcb, 0xe3, 0xa0, 0x9c, 0x08}` |
| 1436 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xfb} );` |
| 1437 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe4, 0x4b, 0xdf, 0xab, 0xa4, 0x22, 0x9e, 0x1a}` |
| 1438 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xe3} );` |
| 1439 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xf3, 0xf8, 0x6e, 0x18, 0xad, 0x35, 0xbb, 0x02}` |
| 1440 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xeb} );` |
| 1441 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe1, 0x58, 0x4f, 0x3e, 0xb2, 0x25, 0xff, 0x08}` |
| 1442 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xd3} );` |
| 1443 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xea, 0x59, 0xdc, 0x3f, 0x92, 0x08, 0x14, 0x01}` |
| 1444 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xdb} );` |
| 1445 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x46, 0x4d, 0x9d, 0xcf, 0xa4, 0x25, 0xbe, 0x77}` |
| 1446 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xc3} );` |
| 1447 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xa5, 0x59, 0x08, 0xb1, 0x93, 0x25, 0x98, 0x93}` |
| 1448 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xcb} );` |
| 1449 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x6a, 0x53, 0x1c, 0xb5, 0x92, 0xc5, 0xb5, 0x1b}` |
| 1450 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xb3} );` |
| 1451 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xf6, 0x67, 0x5a, 0x3f, 0x76, 0xab, 0xb9, 0x0b}` |
| 1452 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xbb} );` |
| 1453 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0x40, 0xcd, 0x22, 0xb2, 0x57, 0xc9, 0xdb}` |
| 1454 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xa3} );` |
| 1455 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xf8, 0x79, 0xa2, 0x17, 0xe7, 0x25, 0x5c, 0x85}` |
| 1456 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0xab} );` |
| 1457 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xc7, 0x59, 0xcc, 0x20, 0xb3, 0x38, 0x18, 0x13}` |
| 1458 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x93} );` |
| 1459 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x97, 0x45, 0xda, 0x1f, 0xea, 0x09, 0xbd, 0x0b}` |
| 1460 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x9b} );` |
| 1461 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0x22, 0xd7, 0xed, 0x3f, 0xb2, 0xbb, 0x98, 0x0b}` |
| 1462 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x83} );` |
| 1463 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0x59, 0xcc, 0x3f, 0xb2, 0x25, 0x98, 0x0b}` |
| 1464 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xa1, 0x08, 0x8b} );` |
| 1465 | `configureCommonBlockC` | `mailRead( 0x33, 8 ); //EXPECTED {0xe6, 0x59, 0xcc, 0x3f, 0xb2, 0x25, 0x98, 0x04}` |
| 1466 | `configureCommonBlockC` | `mailWrite( 0x33, VC{0xaa, 0xb8, 0x29, 0xe7} );` |

## src/gchd/configure_hdmi.cpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_hdmi.cpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 16 | `readHdmiSignalInformation` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 17 | `readHdmiSignalInformation` | `unsigned value6665  = readDevice0x9DCD(0x66) <<8;` |
| 18 | `readHdmiSignalInformation` | `value6665 \|= readDevice0x9DCD(0x65);` |
| 22 | `readHdmiSignalInformation` | `unsigned value6463=readDevice0x9DCD(0x64) <<8;` |
| 23 | `readHdmiSignalInformation` | `value6463 \|= readDevice0x9DCD(0x63);` |
| 27 | `readHdmiSignalInformation` | `mailWrite( 0x4e, VC{0x00, 0xce} ); //bank switch.` |
| 28 | `readHdmiSignalInformation` | `uint8_t value=readDevice0x9DCD(0x34);` |
| 32 | `configureHDMI` | `void GCHD::configureHDMI()` |
| 34 | `configureHDMI` | `mailWrite( 0x33, VC{0x94, 0x41, 0x37} );` |
| 35 | `configureHDMI` | `mailWrite( 0x33, VC{0x94, 0x4a, 0xaf} );` |
| 36 | `configureHDMI` | `mailWrite( 0x33, VC{0x94, 0x4b, 0xaf} );` |
| 37 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 38 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 39 | `configureHDMI` | `readDevice0x9DCD(0x94); //EXPECTED 0xb2` |
| 40 | `configureHDMI` | `mailWrite( 0x4e, VC{0xab, 0x4c} );` |
| 41 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 42 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 43 | `configureHDMI` | `mailWrite( 0x4e, VC{0x1b, 0x33} );` |
| 44 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 45 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 46 | `configureHDMI` | `readDevice0x9DCD(0x88); //EXPECTED 0xb0` |
| 47 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb7, 0xce} );` |
| 48 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb8, 0xdc} );` |
| 49 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb8, 0xcc} );` |
| 50 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 51 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 52 | `configureHDMI` | `mailWrite( 0x4e, VC{0x07, 0x38} );` |
| 53 | `configureHDMI` | `mailWrite( 0x4e, VC{0x07, 0xc8} );` |
| 54 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 55 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 56 | `configureHDMI` | `mailWrite( 0x4e, VC{0x51, 0x45} );` |
| 57 | `configureHDMI` | `readDevice0x9DCD(0x88); //EXPECTED 0xb0` |
| 58 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb7, 0xcc} );` |
| 61 | `configureHDMI` | `doEnable( 0x0000, 0x0000); //state 031e->031e, enable 000a->000a` |
| 62 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 63 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 64 | `configureHDMI` | `readDevice0x9DCD(0x3e); //EXPECTED 0xd3` |
| 65 | `configureHDMI` | `mailWrite( 0x4e, VC{0x01, 0xad} );` |
| 66 | `configureHDMI` | `readDevice0x9DCD(0x3b); //EXPECTED 0xb3` |
| 67 | `configureHDMI` | `mailWrite( 0x4e, VC{0x04, 0xcd} );` |
| 68 | `configureHDMI` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 69 | `configureHDMI` | `readDevice0x9DCD(0x36); //EXPECTED 0xba` |
| 70 | `configureHDMI` | `mailWrite( 0x4e, VC{0x09, 0xe4} );` |
| 71 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 72 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 73 | `configureHDMI` | `readDevice0x9DCD(0x6b); //EXPECTED 0x82` |
| 74 | `configureHDMI` | `mailWrite( 0x4e, VC{0x54, 0xec} );` |
| 75 | `configureHDMI` | `readDevice0x9DCD(0x93); //EXPECTED 0xb2` |
| 76 | `configureHDMI` | `mailWrite( 0x4e, VC{0xac, 0x4c} );` |
| 77 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 78 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0x4c} );` |
| 79 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0x32` |
| 80 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 81 | `configureHDMI` | `readDevice0x9DCD(0xf1); //EXPECTED 0xb2` |
| 82 | `configureHDMI` | `mailWrite( 0x4e, VC{0xce, 0x4c} );` |
| 83 | `configureHDMI` | `readDevice0x9DCD(0xf0); //EXPECTED 0xb2` |
| 84 | `configureHDMI` | `mailWrite( 0x4e, VC{0xcf, 0xce} );` |
| 85 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 170 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 176 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb2, 0xc4} );` |
| 177 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb5, 0xd0} );` |
| 180 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb2, 0xcc} );` |
| 181 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb5, 0xcc} );` |
| 185 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb2, 0xcf} );` |
| 186 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb5, 0xcc} );` |
| 192 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 193 | `configureHDMI` | `mailWrite( 0x4e, VC{0x1b, 0x30} );` |
| 195 | `configureHDMI` | `mailWrite( 0x4e, VC{0x1f, 0xdc} );` |
| 196 | `configureHDMI` | `readDevice0x9DCD(0x29); //EXPECTED 0xba` |
| 197 | `configureHDMI` | `mailWrite( 0x4e, VC{0x1f, 0xcc} );` |
| 198 | `configureHDMI` | `readDevice0x9DCD(0x3b); //EXPECTED 0xb3` |
| 199 | `configureHDMI` | `mailWrite( 0x4e, VC{0x04, 0xcf} );` |
| 200 | `configureHDMI` | `mailWrite( 0x4e, VC{0x04, 0xcd} );` |
| 201 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 202 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 203 | `configureHDMI` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 204 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 205 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcd} );` |
| 207 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 208 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb0, 0xe8} );` |
| 209 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb1, 0x0c} );` |
| 210 | `configureHDMI` | `mailWrite( 0x4e, VC{0xad, 0xc9} );` |
| 211 | `configureHDMI` | `readDevice0x9DCD(0x8f); //EXPECTED 0x96` |
| 212 | `configureHDMI` | `mailWrite( 0x4e, VC{0xb0, 0xe9} );` |
| 213 | `configureHDMI` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 215 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 216 | `configureHDMI` | `mailWrite( 0x4e, VC{0xab, 0xcc} );` |
| 217 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 218 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 219 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 220 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x6f}` |
| 221 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 222 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 223 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfc} );` |
| 224 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 225 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf3} );` |
| 226 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 227 | `configureHDMI` | `mailWrite( 0x4c, VC{0x0c, 0x89} );` |
| 228 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 229 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 230 | `configureHDMI` | `mailWrite( 0x4c, VC{0x0e, 0x65} );` |
| 231 | `configureHDMI` | `mailWrite( 0x4c, VC{0x0e, 0x64} );` |
| 232 | `configureHDMI` | `configureCommonBlockA();` |
| 234 | `configureHDMI` | `configureSetupSubblock();` |
| 236 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0x6b} );` |
| 237 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 238 | `configureHDMI` | `mailWrite( 0x4c, VC{0xc0, 0x80} );` |
| 239 | `configureHDMI` | `mailWrite( 0x4c, VC{0xa2, 0x77} );` |
| 240 | `configureHDMI` | `mailWrite( 0x33, VC{0x99, 0x89, 0x58} );` |
| 241 | `configureHDMI` | `mailRead( 0x33, 1 ); //EXPECTED {0x91}` |
| 242 | `configureHDMI` | `mailWrite( 0x4c, VC{0xc0, 0x77} );` |
| 250 | `configureHDMI` | `configureCommonBlockB1(mysteryParameter);` |
| 251 | `configureHDMI` | `configureCommonBlockB2();` |
| 252 | `configureHDMI` | `configureCommonBlockB3();` |
| 253 | `configureHDMI` | `configureCommonBlockC();` |
| 266 | `configureHDMI` | `mailWrite( 0x4e, VC{0x00, 0xcc} ); //Make sure on right bank for color space configure.` |
| 267 | `configureHDMI` | `configureColorSpace();` |
| 268 | `configureHDMI` | `transcoderFinalConfigure( currentInputSettings_,  currentTranscoderSettings_ );` |
| 269 | `configureHDMI` | `transcoderSetup( currentInputSettings_,  currentTranscoderSettings_ );` |
| 271 | `configureHDMI` | `scmd(SCMD_INIT, 0xa0, 0x0000);` |
| 273 | `configureHDMI` | `scmd(SCMD_STATE_CHANGE, 0x00, SCMD_STATE_START);` |
| 274 | `configureHDMI` | `completeStateChange(state, SCMD_STATE_START); //EXPECTED 0x0001` |

## src/gchd/configure_component.cpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_component.cpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 17 | `readComponentSignalInformation` | `unsigned value6665  = readDevice0x9DCD(0x66) <<8;` |
| 18 | `readComponentSignalInformation` | `value6665 \|= readDevice0x9DCD(0x65);` |
| 22 | `readComponentSignalInformation` | `unsigned value6867  = readDevice0x9DCD(0x68) <<8;` |
| 23 | `readComponentSignalInformation` | `value6867 \|= readDevice0x9DCD(0x67);` |
| 31 | `configureComponent` | `void GCHD::configureComponent()` |
| 33 | `configureComponent` | `mailWrite( 0x33, VC{0x94, 0x41, 0x37} );` |
| 34 | `configureComponent` | `mailWrite( 0x33, VC{0x94, 0x4a, 0xaf} );` |
| 35 | `configureComponent` | `mailWrite( 0x33, VC{0x94, 0x4b, 0xaf} );` |
| 36 | `configureComponent` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 37 | `configureComponent` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 38 | `configureComponent` | `readDevice0x9DCD(0x94); //EXPECTED 0xb2` |
| 39 | `configureComponent` | `mailWrite( 0x4e, VC{0xab, 0x4c} );` |
| 40 | `configureComponent` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 41 | `configureComponent` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 42 | `configureComponent` | `mailWrite( 0x4e, VC{0x1b, 0x33} );` |
| 43 | `configureComponent` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 44 | `configureComponent` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 45 | `configureComponent` | `readDevice0x9DCD(0x88); //EXPECTED 0xb0` |
| 46 | `configureComponent` | `mailWrite( 0x4e, VC{0xb7, 0xce} );` |
| 47 | `configureComponent` | `mailWrite( 0x4e, VC{0xb8, 0xdc} );` |
| 48 | `configureComponent` | `mailWrite( 0x4e, VC{0xb8, 0xcc} );` |
| 49 | `configureComponent` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 50 | `configureComponent` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 51 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0x38} );` |
| 52 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xc8} );` |
| 53 | `configureComponent` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 54 | `configureComponent` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 55 | `configureComponent` | `mailWrite( 0x4e, VC{0x51, 0xed} ); //Changed from HDMI.` |
| 56 | `configureComponent` | `readDevice0x9DCD(0x88); //EXPECTED 0xb0` |
| 57 | `configureComponent` | `mailWrite( 0x4e, VC{0xb7, 0xce} ); //Changed from HDMI` |
| 60 | `configureComponent` | `doEnable( 0x0000, 0x0000); //state 031e->031e, enable 000a->000a` |
| 62 | `configureComponent` | `mailWrite( 0x4e, VC{0x24, 0x0c} );` |
| 63 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xcd} );` |
| 64 | `configureComponent` | `mailWrite( 0x4e, VC{0x91, 0xc8} );` |
| 65 | `configureComponent` | `mailWrite( 0x4e, VC{0x0e, 0x8c} );` |
| 66 | `configureComponent` | `mailWrite( 0x4e, VC{0x11, 0xec} );` |
| 67 | `configureComponent` | `mailWrite( 0x4e, VC{0x71, 0x4c} );` |
| 68 | `configureComponent` | `mailWrite( 0x4e, VC{0x04, 0xcc} ); //THIS HAS DIFFERENT VALUE` |
| 180 | `configureComponent` | `mailWrite( 0x4e, VC{0xb2, 0xcf} );` |
| 183 | `configureComponent` | `mailWrite( 0x4e, VC{0xb2, 0xcc} );` |
| 186 | `configureComponent` | `mailWrite( 0x4e, VC{0xb5, 0xcc} );` |
| 193 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x94} );` |
| 194 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xf4} );` |
| 195 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xec} );` |
| 196 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xfc} );` |
| 197 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0xdd} );` |
| 198 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xcd} );` |
| 199 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0x45} );` |
| 200 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0xbc} );` |
| 202 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x24} );` |
| 203 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xf4} );` |
| 204 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xfc} );` |
| 205 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xac} );` |
| 206 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0xcc} );` |
| 207 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xcc} );` |
| 208 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0x45} );` |
| 209 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0xbc} );` |
| 214 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x94} );` |
| 215 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xf4} );` |
| 216 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xec} );` |
| 217 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xfc} );` |
| 218 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0xdd} );` |
| 219 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xcd} );` |
| 220 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0xab} );` |
| 221 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0xdc} );` |
| 226 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xc4} );` |
| 227 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xc4} );` |
| 228 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 229 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xd4} );` |
| 230 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0x88} );` |
| 231 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xc8} );` |
| 232 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0xf9} );` |
| 233 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0x3c} );` |
| 235 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xd4} );` |
| 236 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xc4} );` |
| 237 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 238 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xd4} );` |
| 239 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0xff} );` |
| 240 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xcf} );` |
| 241 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0xf9} );` |
| 242 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0x3c} );` |
| 247 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xc4} );` |
| 248 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xc4} );` |
| 249 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 250 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xd4} );` |
| 251 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0x88} );` |
| 252 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xc8} );` |
| 253 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0xf9} );` |
| 254 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0x5c} );` |
| 256 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xec} );` |
| 257 | `configureComponent` | `mailWrite( 0x4e, VC{0x05, 0xc4} );` |
| 258 | `configureComponent` | `mailWrite( 0x4e, VC{0x06, 0xc4} );` |
| 259 | `configureComponent` | `mailWrite( 0x4e, VC{0x07, 0xd4} );` |
| 260 | `configureComponent` | `mailWrite( 0x4e, VC{0x1e, 0xff} );` |
| 261 | `configureComponent` | `mailWrite( 0x4e, VC{0x1f, 0xcf} );` |
| 262 | `configureComponent` | `mailWrite( 0x4e, VC{0x01, 0xf9} );` |
| 263 | `configureComponent` | `mailWrite( 0x4e, VC{0x02, 0x5c} );` |
| 270 | `configureComponent` | `readDevice0x9DCD(0x2f); //EXPECTED 0x0a` |
| 271 | `configureComponent` | `mailWrite( 0x4e, VC{0x10, 0x71} );` |
| 272 | `configureComponent` | `readDevice0x9DCD(0x30); //EXPECTED 0xba` |
| 273 | `configureComponent` | `mailWrite( 0x4e, VC{0x0f, 0xe4} );` |
| 274 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 275 | `configureComponent` | `readDevice0x9DCD(0x28); //EXPECTED 0xb2` |
| 276 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xce} );` |
| 280 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 283 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xcc} );` |
| 286 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xd8} );` |
| 287 | `configureComponent` | `mailWrite( 0x4e, VC{0x0b, 0x4c} );` |
| 288 | `configureComponent` | `mailWrite( 0x4e, VC{0x0c, 0xac} );` |
| 289 | `configureComponent` | `mailWrite( 0x4e, VC{0x0d, 0x4c} );` |
| 290 | `configureComponent` | `mailWrite( 0x4e, VC{0x1b, 0xcc} );` |
| 291 | `configureComponent` | `mailWrite( 0x4e, VC{0x1c, 0xcc} );` |
| 292 | `configureComponent` | `mailWrite( 0x4e, VC{0x1d, 0xcc} );` |
| 293 | `configureComponent` | `mailWrite( 0x4e, VC{0x18, 0xdc} );` |
| 294 | `configureComponent` | `mailWrite( 0x4e, VC{0x19, 0xdc} );` |
| 295 | `configureComponent` | `mailWrite( 0x4e, VC{0x1a, 0xdc} );` |
| 296 | `configureComponent` | `mailWrite( 0x4e, VC{0x2d, 0xdd} );` |
| 297 | `configureComponent` | `mailWrite( 0x4e, VC{0x2e, 0xdd} );` |
| 298 | `configureComponent` | `mailWrite( 0x4e, VC{0x2f, 0xce} );` |
| 299 | `configureComponent` | `mailWrite( 0x4e, VC{0x3a, 0xc0} );` |
| 300 | `configureComponent` | `mailWrite( 0x4e, VC{0x3b, 0xc4} );` |
| 301 | `configureComponent` | `readDevice0x9DCD(0x39); //EXPECTED 0x92/0x82 first is HD1080i60, second 108p30  might be interlace bit` |
| 305 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x2c} );` |
| 306 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 307 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 308 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 309 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xd8} );` |
| 310 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 311 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 312 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 313 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 314 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 315 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0x73} );` |
| 316 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xcb} );` |
| 317 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x4c} );` |
| 318 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xdf} );` |
| 319 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xee} );` |
| 320 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0xd0} );` |
| 322 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x3c} );` |
| 323 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 324 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x24} );` |
| 325 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xcc} );` |
| 326 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xc6} );` |
| 327 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 328 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xcb} );` |
| 329 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 330 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 331 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 332 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0x73} );` |
| 333 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xcb} );` |
| 334 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x4c} );` |
| 335 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xe4} );` |
| 336 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xc8} );` |
| 337 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0xf4} );` |
| 341 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x2c} );` |
| 342 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 343 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 344 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 345 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xd8} );` |
| 346 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 347 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 348 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 349 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 350 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcd} );` |
| 351 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0xcf} );` |
| 352 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xc9} );` |
| 353 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0xcc} );` |
| 354 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xd4} );` |
| 355 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xce} );` |
| 356 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0x1c} );` |
| 360 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x54} );` |
| 361 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 362 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xcc} );` |
| 363 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 364 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xc2} );` |
| 365 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc6} );` |
| 366 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 367 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xec} );` |
| 368 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xc4} );` |
| 369 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 370 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0x4f} );` |
| 371 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xce} );` |
| 372 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x1c} );` |
| 373 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xd9} );` |
| 374 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xed} );` |
| 375 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0xec} );` |
| 377 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x54} );` |
| 378 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 379 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 380 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 381 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xe4} );` |
| 382 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 383 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 384 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 385 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 386 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 387 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0x8f} );` |
| 388 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xce} );` |
| 389 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x1c} );` |
| 390 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xe7} );` |
| 391 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xce} );` |
| 392 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0x8c} );` |
| 397 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x54} );` |
| 398 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 399 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0xcc} );` |
| 400 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 401 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xd8} );` |
| 402 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 403 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 404 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xec} );` |
| 405 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xc4} );` |
| 406 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 407 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0xf4} );` |
| 408 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xce} );` |
| 409 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x1c} );` |
| 410 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xdd} );` |
| 411 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xec} );` |
| 412 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0x3c} );` |
| 414 | `configureComponent` | `mailWrite( 0x4e, VC{0x39, 0x54} );` |
| 415 | `configureComponent` | `mailWrite( 0x4e, VC{0x2c, 0x51} );` |
| 416 | `configureComponent` | `mailWrite( 0x4e, VC{0x03, 0x8c} );` |
| 417 | `configureComponent` | `mailWrite( 0x4e, VC{0x12, 0xc8} );` |
| 418 | `configureComponent` | `mailWrite( 0x4e, VC{0x13, 0xec} );` |
| 419 | `configureComponent` | `mailWrite( 0x4e, VC{0x17, 0xc7} );` |
| 420 | `configureComponent` | `mailWrite( 0x4e, VC{0x21, 0xc6} );` |
| 421 | `configureComponent` | `mailWrite( 0x4e, VC{0x20, 0xcc} );` |
| 422 | `configureComponent` | `mailWrite( 0x4e, VC{0x40, 0xcc} );` |
| 423 | `configureComponent` | `mailWrite( 0x4e, VC{0x80, 0xcc} );` |
| 424 | `configureComponent` | `mailWrite( 0x4e, VC{0x81, 0xf7} );` |
| 425 | `configureComponent` | `mailWrite( 0x4e, VC{0x82, 0xce} );` |
| 426 | `configureComponent` | `mailWrite( 0x4e, VC{0x83, 0x1c} );` |
| 427 | `configureComponent` | `mailWrite( 0x4e, VC{0x84, 0xef} );` |
| 428 | `configureComponent` | `mailWrite( 0x4e, VC{0x85, 0xcd} );` |
| 429 | `configureComponent` | `mailWrite( 0x4e, VC{0x86, 0x2c} );` |
| 437 | `configureComponent` | `mailWrite( 0x4e, VC{0xb0, 0xe8} );` |
| 438 | `configureComponent` | `mailWrite( 0x4e, VC{0xb1, 0x0c} );` |
| 441 | `configureComponent` | `mailWrite( 0x4e, VC{0xad, 0xcc} );` |
| 442 | `configureComponent` | `mailWrite( 0x4e, VC{0xb0, 0xf9} );` |
| 443 | `configureComponent` | `mailWrite( 0x4e, VC{0xb1, 0xcc} );` |
| 445 | `configureComponent` | `mailWrite( 0x4e, VC{0xad, 0xcc} );` |
| 446 | `configureComponent` | `mailWrite( 0x4e, VC{0xb0, 0xf9} );` |
| 447 | `configureComponent` | `mailWrite( 0x4e, VC{0xb1, 0x4c} );` |
| 449 | `configureComponent` | `mailWrite( 0x4e, VC{0xad, 0xc9} );` |
| 450 | `configureComponent` | `mailWrite( 0x4e, VC{0xb0, 0xe9} );` |
| 457 | `configureComponent` | `configureColorSpace();` |
| 460 | `configureComponent` | `mailWrite( 0x4e, VC{0xb0, 0xf9} );` |
| 461 | `configureComponent` | `mailWrite( 0x4e, VC{0xb1, 0xcc} );` |
| 464 | `configureComponent` | `mailWrite( 0x4e, VC{0xab, 0xcc} );` |
| 465 | `configureComponent` | `mailWrite( 0x4c, VC{0x04, 0x95} );` |
| 467 | `configureComponent` | `configureSetupSubblock();` |
| 468 | `configureComponent` | `configureCommonBlockB1( false );` |
| 470 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 471 | `configureComponent` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 472 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 473 | `configureComponent` | `mailRead( 0x33, 1 ); //EXPECTED {0x6f}` |
| 474 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 475 | `configureComponent` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 476 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfc} );` |
| 477 | `configureComponent` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 478 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf3} );` |
| 479 | `configureComponent` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 480 | `configureComponent` | `mailWrite( 0x4c, VC{0x0c, 0x89} );` |
| 482 | `configureComponent` | `mailWrite( 0x4c, VC{0x0d, 0xc8} ); //This isn't in all captures at this point, but usually appears soon later` |
| 485 | `configureComponent` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 486 | `configureComponent` | `uint8_t status=mailRead( 0x33, 1 )[0];` |
| 489 | `configureComponent` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x65)} );` |
| 490 | `configureComponent` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x64)} );` |
| 492 | `configureComponent` | `configureCommonBlockA();` |
| 493 | `configureComponent` | `configureSetupSubblock();` |
| 501 | `configureComponent` | `configureCommonBlockB1(mysteryParameter);` |
| 502 | `configureComponent` | `configureCommonBlockB2();` |
| 503 | `configureComponent` | `configureCommonBlockB3();` |
| 505 | `configureComponent` | `mailWrite( 0x4e, VC{0x0f, 0xe4} );` |
| 510 | `configureComponent` | `//   * readDevice0x9DCD(??)` |
| 511 | `configureComponent` | `//   * mailWrite( 0x4e, VC{ 0x04, ??} )` |
| 512 | `configureComponent` | `//   * mailWrite( 0x4e, VC{ 0xe0, 6c} )` |
| 513 | `configureComponent` | `//   * mailWrite( 0x4e, VC{ 0xe0, 8c} )` |
| 514 | `configureComponent` | `//   * mailWrite( 0x4e, VC{ 0xe0, cc} )` |
| 515 | `configureComponent` | `//   * mailWrite( 0x4e, VC{ 0xe0, ec} )` |
| 516 | `configureComponent` | `//   * mailWrite( 0x33, VC{0x99, 0x89, 0xf5} ); followed by a` |
| 517 | `configureComponent` | `//   * mailRead( 0x33, 1 );` |
| 519 | `configureComponent` | `configureCommonBlockC();` |
| 521 | `configureComponent` | `transcoderFinalConfigure( currentInputSettings_ , currentTranscoderSettings_ );` |
| 522 | `configureComponent` | `transcoderSetup( currentInputSettings_, currentTranscoderSettings_ );` |
| 524 | `configureComponent` | `scmd(SCMD_INIT, 0xa0, 0x0000);` |
| 526 | `configureComponent` | `scmd(SCMD_STATE_CHANGE, 0x00, SCMD_STATE_START);` |
| 527 | `configureComponent` | `completeStateChange(state, SCMD_STATE_START); //EXPECTED 0x0001` |

## src/gchd/configure_composite.cpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd/configure_composite.cpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 13 | `configureComposite` | `void GCHD::configureComposite()` |
| 15 | `configureComposite` | `mailWrite( 0x33, VC{0x94, 0x41, 0x37} );` |
| 16 | `configureComposite` | `mailWrite( 0x33, VC{0x94, 0x4a, 0xaf} );` |
| 17 | `configureComposite` | `mailWrite( 0x33, VC{0x94, 0x4b, 0xaf} );` |
| 20 | `configureComposite` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfa} );` |
| 21 | `configureComposite` | `uint8_t value=mailRead( 0x33, 1 )[0];` |
| 68 | `configureComposite` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 69 | `configureComposite` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 70 | `configureComposite` | `readDevice0x9DCD(0x94); //EXPECTED 0xb2` |
| 71 | `configureComposite` | `mailWrite( 0x4e, VC{0xab, 0x4c} );` |
| 72 | `configureComposite` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 73 | `configureComposite` | `mailWrite( 0x4e, VC{0x00, 0xce} );` |
| 74 | `configureComposite` | `mailWrite( 0x4e, VC{0x1b, 0x33} );` |
| 75 | `configureComposite` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb0` |
| 76 | `configureComposite` | `mailWrite( 0x4e, VC{0x00, 0xcc} );` |
| 77 | `configureComposite` | `readDevice0x9DCD(0x88); //EXPECTED 0xb0` |
| 78 | `configureComposite` | `mailWrite( 0x4e, VC{0xb7, 0xce} );` |
| 79 | `configureComposite` | `mailWrite( 0x4e, VC{0xb8, 0xdc} );` |
| 80 | `configureComposite` | `mailWrite( 0x4e, VC{0xb8, 0xcc} );` |
| 81 | `configureComposite` | `readDevice0x9DCD(0x3f); //EXPECTED 0xb2` |
| 86 | `configureComposite` | `mailWrite( 0x44, VC{0x07, 0x8a} );` |
| 87 | `configureComposite` | `mailWrite( 0x44, VC{0x08, 0x9b} );` |
| 88 | `configureComposite` | `mailWrite( 0x44, VC{0x09, 0x7a} );` |
| 89 | `configureComposite` | `mailWrite( 0x44, VC{0x28, 0x88} );` |
| 90 | `configureComposite` | `mailWrite( 0x33, VC{0x89, 0x89, 0xfd} );` |
| 91 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 92 | `configureComposite` | `mailWrite( 0x44, VC{0x06, 0x08} );` |
| 94 | `configureComposite` | `mailWrite( 0x33, VC{0x89, 0x89, 0xe7} );` |
| 95 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0xe9}` |
| 98 | `configureComposite` | `mailWrite( 0x33, VC{0x89, 0x89, 0xf1} );` |
| 99 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x63}` |
| 100 | `configureComposite` | `mailWrite( 0x4c, VC{0x04, 0x95} );` |
| 105 | `configureComposite` | `configureSetupSubblock();` |
| 106 | `configureComposite` | `configureCommonBlockB1( false );` |
| 109 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 110 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 111 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfd} );` |
| 112 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x6f}` |
| 113 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 114 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x82}` |
| 115 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xfc} );` |
| 116 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 117 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf3} );` |
| 118 | `configureComposite` | `mailRead( 0x33, 1 ); //EXPECTED {0x6e}` |
| 120 | `configureComposite` | `mailWrite( 0x4c, VC{0x0c, 0x89} );` |
| 121 | `configureComposite` | `mailWrite( 0x4c, VC{0x0d, 0xc8} ); //This isn't in all captures at this point, but usually appears soon later` |
| 124 | `configureComposite` | `mailWrite( 0x33, VC{0x99, 0x89, 0xf5} );` |
| 125 | `configureComposite` | `uint8_t status=mailRead( 0x33, 1 )[0];` |
| 128 | `configureComposite` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x65)} );` |
| 129 | `configureComposite` | `mailWrite( 0x4c, VC{0x0e, (uint8_t) (mask \| 0x64)} );` |
| 132 | `configureComposite` | `configureCommonBlockA();` |
| 133 | `configureComposite` | `configureSetupSubblock();` |
| 134 | `configureComposite` | `configureCommonBlockB1(false);` |
| 135 | `configureComposite` | `configureCommonBlockB2();` |
| 136 | `configureComposite` | `configureCommonBlockB3();` |
| 138 | `configureComposite` | `configureCommonBlockC();` |
| 139 | `configureComposite` | `configureCommonBlockB3(); //Pretty sure not necessary to repeat.` |
| 141 | `configureComposite` | `transcoderFinalConfigure( currentInputSettings_ , currentTranscoderSettings_ );` |
| 142 | `configureComposite` | `transcoderSetup( currentInputSettings_, currentTranscoderSettings_ );` |
| 144 | `configureComposite` | `scmd(SCMD_INIT, 0xa0, 0x0000);` |
| 146 | `configureComposite` | `scmd(SCMD_STATE_CHANGE, 0x00, SCMD_STATE_START);` |
| 147 | `configureComposite` | `completeStateChange(state, SCMD_STATE_START); //EXPECTED 0x0001` |

## src/gchd_hardware.hpp

Source order is preserved. Line numbers refer to the exact file revision linked below. Calls inside loops/branches remain conditional; this ledger does not flatten them into a replay-safe sequence.

[Open source](https://github.com/FF-5/elgato-gchd/blob/diagnostic-input-baseline/src/gchd_hardware.hpp)

| Line | Enclosing function | Literal call / operation |
|---:|---|---|
| 328 | `(global / constants)` | `* for more documentation, see the scmd() method of gchd in commands.cpp` |
| 362 | `(global / constants)` | `* Then the driver code explicitly calls scmd(SCMD_IDLE, 0,0), which has the` |
| 409 | `(global / constants)` | `* This appears to only be used in scmd() calls that trigger` |
| 410 | `(global / constants)` | `* an interrupt, see the scmd() code for more details.` |
