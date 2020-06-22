#include <3ds.h>
#include <stdio.h>

#define NN_ATTRIBUTE_ALIGN(n) __attribute__ ((aligned(n)))

u8 extraPadWorkingMemory[4096] NN_ATTRIBUTE_ALIGN(4096);
int irWorkingMemory = 0;
char peripheralId = 0x01;

Handle iruserHandle;
Handle iruserSharedMemHandle;
Handle connectionStatusEvent;

bool connected = false;
Result initializeIrnopSharedResult;
Result getConnectionStatusEventResult;
Result requireConnectionResult;

Result InitializeIrnopShared()
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x00180182;
    cmdbuf[1] = 4096u;
    cmdbuf[2] = 3280u;
    cmdbuf[3] = (unsigned int)(0x00000500u >> 3);
    cmdbuf[4] = (unsigned int)(512 + 256);
    cmdbuf[5] = (unsigned int)(256u >> 3);
    cmdbuf[6] = (char)0x04; //byte 0x04
    cmdbuf[7] = 0;
    cmdbuf[8] = iruserSharedMemHandle; // mem handle

    if (R_FAILED(ret = svcSendSyncRequest(iruserHandle))) return ret;
    ret = (Result)cmdbuf[1];

    return ret;
}

Result GetConnectionStatusEvent(Handle *event_)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x000C0000;

    if (R_FAILED(ret = svcSendSyncRequest(iruserHandle))) return ret;
    ret = (Result)cmdbuf[1];

    *event_ = cmdbuf[3];

    return ret;
}

Result RequireConnection()
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x00060040;
    cmdbuf[1] = peripheralId;

    if (R_FAILED(ret = svcSendSyncRequest(iruserHandle))) return ret;
    ret = (Result)cmdbuf[1];

    return ret;
}

Result Disconnect()
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = 0x00090000;

    if (R_FAILED(ret = svcSendSyncRequest(iruserHandle))) return ret;
    ret = (Result)cmdbuf[1];

    return ret;
}

int out, out_;
int StartSampling(int samplingThreadPriority, int period) // Button A
{
    int result = -1;
    // cepdSetPeriod(...);

    // Get IR:USER Handle
    Result ret = 0;
    ret = srvGetServiceHandle(&iruserHandle, "ir:USER");
    if (R_FAILED(ret)) iruserHandle = 0xFFFFFFFF;

    ret = svcCreateMemoryBlock(&iruserSharedMemHandle, irWorkingMemory, 0x1000, 1u, 3u);
    if (R_FAILED(ret)) iruserSharedMemHandle = 0xFFFFFFFF;

    initializeIrnopSharedResult = InitializeIrnopShared();

    svcCreateEvent(&connectionStatusEvent, 0);
    getConnectionStatusEventResult = GetConnectionStatusEvent(&connectionStatusEvent);

    svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 0);
    requireConnectionResult = RequireConnection();
    svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 4000000);

    /*while (true)
    {
        svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 0);
        requireConnectionResult = RequireConnection();
        int i = svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 4000000);
        s16 s = i;
        int i_ = s & 0x3FF;
        if (i_ != 1022) break;
        u8 connectionStatus = *((u8 *)(irWorkingMemory + 8));
        if (connectionStatus == 2) break;
        Disconnect();
        svcSleepThread(10416);
    }*/

    u8 connectionStatus = *((u8 *)(irWorkingMemory + 8));

    if (connectionStatus == 2) connected = true;

    out = connectionStatus;

    return result;
}

void TryToConnect() // Button B
{
    svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 0);
    RequireConnection();
    svcWaitSynchronizationN(&out_, &connectionStatusEvent, 1, 0, 4000000);

    u8 connectionStatus = *((u8 *)(irWorkingMemory + 8));

    if (connectionStatus == 2) connected = true;

    out = connectionStatus;

    Disconnect();
}

int main(int argc, char **argv)
{
    //Matrix containing the name of each key. Useful for printing when a key is pressed
    char keysNames[32][32] = {
        "KEY_A", "KEY_B", "KEY_SELECT", "KEY_START",
        "KEY_DRIGHT", "KEY_DLEFT", "KEY_DUP", "KEY_DDOWN",
        "KEY_R", "KEY_L", "KEY_X", "KEY_Y",
        "", "", "KEY_ZL", "KEY_ZR",
        "", "", "", "",
        "KEY_TOUCH", "", "", "",
        "KEY_CSTICK_RIGHT", "KEY_CSTICK_LEFT", "KEY_CSTICK_UP", "KEY_CSTICK_DOWN",
        "KEY_CPAD_RIGHT", "KEY_CPAD_LEFT", "KEY_CPAD_UP", "KEY_CPAD_DOWN"
    };

    // Initialize services
    gfxInitDefault();

    //Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
    consoleInit(GFX_TOP, NULL);

    u32 kDownOld = 0, kHeldOld = 0, kUpOld = 0; //In these variables there will be information about keys detected in the previous frame

    printf("\x1b[0;0HPress Start to exit.");
    printf("\x1b[1;0HCirclePad position:");
    printf("\x1b[20;0Hir:USER service handle: %x", iruserHandle);
    printf("\x1b[21;0HShared memory handle: %x", iruserSharedMemHandle);
    printf("\x1b[22;0HIIS result: %x", initializeIrnopSharedResult);
    printf("\x1b[23;0HGCSE result: %x", getConnectionStatusEventResult);
    printf("\x1b[24;0HConnectionStatus event handle: %x", connectionStatusEvent);
    printf("\x1b[25;0HRC result: %x", requireConnectionResult);
    if (connected)
        printf("\x1b[27;0HCONNECTED!!! %x", out);
    else
        printf("\x1b[27;0HNot connected. %x", out);

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------

    irWorkingMemory = (int)extraPadWorkingMemory;
    out = *((u8 *)(irWorkingMemory + 8));

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------------------------------------------------------------------------------

    // Main loop
    while (aptMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u32 kDown = hidKeysDown();
        //hidKeysHeld returns information about which buttons have are held down in this frame
        u32 kHeld = hidKeysHeld();
        //hidKeysUp returns information about which buttons have been just released
        u32 kUp = hidKeysUp();

        if (kDown & KEY_START) break; // break in order to return to hbmenu

        //Do the keys printing only if keys have changed
        if (kDown != kDownOld || kHeld != kHeldOld || kUp != kUpOld)
        {
            //Clear console
            consoleClear();

            //These two lines must be rewritten because we cleared the whole console
            printf("\x1b[0;0HPress Start to exit.");
            printf("\x1b[1;0HCirclePad position:");
            printf("\x1b[20;0Hir:USER service handle: %x", iruserHandle);
            printf("\x1b[21;0HShared memory handle: %x", iruserSharedMemHandle);
            printf("\x1b[22;0HIIS result: %x", initializeIrnopSharedResult);
            printf("\x1b[23;0HGCSE result: %x", getConnectionStatusEventResult);
            printf("\x1b[24;0HConnectionStatus event handle: %x", connectionStatusEvent);
            printf("\x1b[25;0HRC result: %x", requireConnectionResult);
            if (connected)
                printf("\x1b[27;0HCONNECTED!!! %x", out);
            else
                printf("\x1b[27;0HNot connected. %x", out);

            printf("\x1b[3;0H"); //Move the cursor to the fourth row because on the third one we'll write the circle pad position

            //Check if some of the keys are down, held or up
            int i;
            for (i = 0; i < 32; i++)
            {
                if (kDown & BIT(i)) printf("%s down\n", keysNames[i]);
                if (kHeld & BIT(i)) printf("%s held\n", keysNames[i]);
                if (kUp & BIT(i)) printf("%s up\n", keysNames[i]);
            }

            if (kDown & KEY_A)
            {
                StartSampling(0, 8);
            }
            if (kDown & KEY_B)
            {
                TryToConnect();
            }
        }

        //Set keys old values for the next frame
        kDownOld = kDown;
        kHeldOld = kHeld;
        kUpOld = kUp;

        circlePosition pos;

        //Read the CirclePad position
        hidCircleRead(&pos);

        //Print the CirclePad position
        printf("\x1b[2;0H%04d; %04d", pos.dx, pos.dy);

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

        //Wait for VBlank
        gspWaitForVBlank();
    }

    // Exit services
    gfxExit();
    return 0;
}
