/**
* Custom Application Tokens
*/
#define CREATOR_REBOOT_MONITOR  (0x000c)

#ifdef DEFINETYPES
// Include or define any typedef for tokens here
#endif //DEFINETYPES
#ifdef DEFINETOKENS
// Define the actual token storage information here

DEFINE_COUNTER_TOKEN(REBOOT_MONITOR, tokTypeStackBootCounter, 0)
//DEFINE_BASIC_TOKEN(REBOOT_MONITOR, uint32_t, 0)

#endif //DEFINETOKENS


