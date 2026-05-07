//The device should run at 60 fps

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <cstring>
#define SCREEN_WIDTH 1280 
#define SCREEN_HEIGHT 720


//SDL Container object for window and renderer
struct sdl_t {
    SDL_Window *window;
    SDL_Renderer *renderer; 
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
}; 

// Emulator States
enum emulator_state_t {
    QUIT, RUNNING, PAUSED
};

//CHIP8 Extensions
enum extension_t {
    CHIP8,
    SUPERCHIP,
    XOCHIP,
};


//Object for storing the configurations
struct config_t {
    uint32_t window_width; 
    uint32_t window_height;
    uint32_t fg_color; //RGBA 8888 //number of bits 
    uint32_t bg_color;
    uint32_t scale_factor;  //Amount to Scale a CHIP8 pixel by
    bool pixel_outlines; // Draw pixel outlines Yes or NO
    uint32_t inst_per_second; //CHIP8 CPU "clock_rate"
    uint32_t square_wave_freq; //Frequency of square wave sound
    uint32_t audio_sample_rate; 
    int16_t volume; // How loud the sound is
    float color_lerp_rate; //Amount to lerp colors by, between [0.1, 1.0];
    extension_t current_extension; //Current extension support. 
};


struct instruction_t {
    uint16_t opcode;
    uint16_t NNN; //12 bit address for constant
    uint8_t   NN; // 8 bit constant 
    uint8_t    N; // 4 bit constant
    uint8_t    X; // 4 bit register identifier
    uint8_t    Y; // 4 bit register identifier. 
     
};

// CHIP8 Machine Object 
struct chip8_t {
    emulator_state_t state; 
    uint8_t ram[4096]; 
    bool display[64*32]; // Hardcoded memory for pixel changes. 
    uint32_t pixel_color[64*32]; //CHIP8 pixel colors to draw. 
    uint16_t stack[12];  //These are modifications, These were supposed to be pointers to locations in RAM. 
    uint16_t *stack_ptr;
    uint8_t  V[16];      // 16 data registers called V
    uint16_t I;          // Index Registers.
    uint16_t PC;         //Program Counter
    uint8_t delay_timer; // Decrements at 60Hz when above 0
    uint8_t sound_timer; // Decrements at 60Hz when above 0 and will play a tone 
    //Now we have 3 different memories for different functions, so in this hardware, we're getting more memory than the actual ones had. 
    bool keypad[16]; //Hexadecimal Keypad 0 to F. 
    const char *rom_name;  // Currently running ROM. 
    instruction_t inst;     // Currently executing instructions. 
    bool draw;          // Draw to the screen or update the screen. 
    
};

// Color Lerp helper function

uint32_t color_lerp(const uint32_t start_color, const uint32_t end_color, float t){
    //Separated Values of Start Color
    const uint8_t s_r = (start_color >> 24) & 0xFF;
    const uint8_t s_g = (start_color >> 16) & 0xFF;
    const uint8_t s_b = (start_color >> 8) & 0xFF;
    const uint8_t s_a = (start_color >> 0) & 0xFF;

    //Separated Values of End Colors. 
    const uint8_t e_r = (end_color >> 24) & 0xFF;
    const uint8_t e_g = (end_color >> 16) & 0xFF;
    const uint8_t e_b = (end_color >> 8) & 0xFF;
    const uint8_t e_a = (end_color >> 0) & 0xFF;


    const uint8_t ret_r = ((1-t)*s_r) + (t*e_r);
    const uint8_t ret_g = ((1-t)*s_g) + (t*e_g);
    const uint8_t ret_b = ((1-t)*s_b) + (t*e_b);
    const uint8_t ret_a = ((1-t)*s_a) + (t*e_a);

    return (ret_r << 24) | (ret_g << 16) | (ret_b << 8)| (ret_a);

}

//SDL Audio Callback function
void audio_callback (void *userdata, uint8_t *stream, int len){
    config_t *config = (config_t *) userdata;

    int16_t *audio_data = (int16_t *) stream;
    static uint32_t running_sample_index = 0; //Static variable means it'll preserve its value across function calls. So if say a everytime a function is called
    //Its value get incremented by 10. If you call it 5 times, it will show 50 at the end, because it preserved the old values from previous calls. 
    const int32_t square_wave_period = config->audio_sample_rate / config->square_wave_freq ; 
    const int32_t half_square_wave_period = square_wave_period/2 ;

    for (int i = 0; i < len/2; i++){
        audio_data[i] = ((running_sample_index++/half_square_wave_period) % 2) ? config->volume : -config->volume;
    }
}


bool initialize_SDL(sdl_t *sdl, config_t *config){
    //Initializes the SDL libraries according to the instructions. 
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
        SDL_Log("Could Not initialize SDL subsystems! %s\n", SDL_GetError());
        return false; 
    }

    sdl -> window = SDL_CreateWindow("CHIP8 Emulator", 
                                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                            config->window_width*config->scale_factor, config->window_height * config->scale_factor, 0);
    
    if (!sdl->window){
        SDL_Log("Could Not create Window %s\n", SDL_GetError()); 
    }
    
    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("Could Not create Renderer %s\n", SDL_GetError());
        return false; 
    
    }

    //initialize Audio stuff

    sdl->want = (SDL_AudioSpec){
        .freq = 44100,
        .format = AUDIO_S16LSB, //Signed 16 bit little endian.
        .channels = 1, 
        .samples = 512,
        .callback = audio_callback,
        .userdata = config,
    };

    sdl->dev = SDL_OpenAudioDevice(NULL,0,&sdl->want, &sdl->have, 0);

    if (sdl->dev == 0){
        SDL_Log("Could not get an audio device %s\n ", SDL_GetError());
        return false; 
    }

    if ((sdl->want.channels != sdl->have.channels ) || (sdl->want.format != sdl->have.format)){
        SDL_Log("Could not get desired Audio Spec\n");
        return false;
    }

    return true;
}
 
//Set up initial emulator configuration from passed in arguments 

bool set_config (config_t *config, const int argc, char **argv){
    //Set Defaults
    *config = (config_t){
        .window_width = 64,  //CHIP_8 Original Width
        .window_height = 32, //CHIP_8 original Height
        .fg_color = 0x38AECCFF, 
        .bg_color = 0x022F40FF, //Yellow initially, testing if its showing yellow. If its not, then the ordering is different. 
        .scale_factor = 20,
        .pixel_outlines = true, // Draw pixel outlines by default. 
        .inst_per_second = 1000, // Number of instructions to emulate in 1 second (clock rate of CPU)
        .square_wave_freq = 440, 
        .audio_sample_rate = 44100, //CD quality
        .volume = 3000, 
        .color_lerp_rate = 0.7, //Color lerp rate.
        .current_extension = CHIP8,
    };
 

    //Overwrite defaults
    for (int i = 1; i < argc ; i++){
        (void) argv[i];
        // e.g: set scale factor
        if(strncmp(argv[i], "--scale-factor", strlen("--scale-factor")) == 0){
            i++;
            config->scale_factor = (uint32_t)strtol(argv[i], NULL, 10);
        }
    }

    return true; 
}

// Initialize Chip 8 Machine 

bool init_chip8(chip8_t *chip8, const config_t *config ,const char rom_name[]){
    const uint32_t entry_point = 0x200; 
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
    };
    //Load Font
    //Initialize the entire chip8 machine
    memset(chip8,0,sizeof(chip8_t)); 

    memcpy(&chip8->ram[0], font, sizeof(font)); //This is an interesting function, does it do the deep copy? 

    //Load ROM to chip8 memory

    //Open the ROM file

    FILE *rom = fopen(rom_name, "rb"); //This is a file pointer. Insane really. 
    if (!rom){
        SDL_Log("ROM file %s is invalid or does not exist \n", rom_name);
        return false;
    }
    //Get and check ROM Size. 
    fseek(rom, 0, SEEK_END);  

    const size_t rom_size = ftell(rom); //number of bytes that we need to read from the ROM into the RAM.
    const size_t max_size = sizeof(chip8->ram) - entry_point; 
    rewind(rom);

    if (rom_size> max_size) {
        SDL_Log("Rom file %s is too big! Rom size: %llu, Max size allowed: %llu \n", rom_name, (unsigned long long)rom_size, (unsigned long long)max_size);
        return false; 
    }
    if(fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1){
        SDL_Log ("Could not read ROM file %s into CHIP8 memory \n", rom_name);
        return false;
    }

    fclose(rom); 

    //Set chip8 machine defaults. 
    chip8->state = RUNNING; // Default machine state is running
    chip8->PC = entry_point; //Start program counter at ROM entry point. 
    chip8->rom_name = rom_name; 
    chip8->stack_ptr = &chip8->stack[0]; 

    memset(&chip8->pixel_color[0], config->bg_color, sizeof chip8->pixel_color); //Init pixels background color

    return true; 
}

void final_cleanup(const sdl_t sdl){
    //Cleans up any memory allocations and any other memory management issues. 
    
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_CloseAudioDevice(sdl.dev);
    
    SDL_Quit(); //Shuts down SDL subsystems
}

//Clear Screen of the SDL window to background color 

void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg_color >> 24) & 0xFF; 
    const uint8_t g = (config.bg_color >> 16) & 0xFF; 
    const uint8_t b = (config.bg_color >>  8) & 0xFF; 
    const uint8_t a = (config.bg_color >>  0) & 0xFF; 

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

//Update window with any changes
void Update_Screen(const sdl_t sdl, chip8_t *chip8, const config_t config){
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    // Grab color values to draw
    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF; 
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF; 
    const uint8_t bg_b = (config.bg_color >>  8) & 0xFF; 
    const uint8_t bg_a = (config.bg_color >>  0) & 0xFF; 

    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF; 
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF; 
    const uint8_t fg_b = (config.fg_color >>  8) & 0xFF; 
    const uint8_t fg_a = (config.fg_color >>  0) & 0xFF; 


    //Loop through all pixels, draw a rectangle per pixel to the SDL window
    for (uint32_t i = 0; i< sizeof(chip8->display); i++){
        //Translate 1D index i value to 2D X/Y coordinates
        // X = i % window_width
        // Y = i /window_width

        rect.x = (i % config.window_width) * config.scale_factor; 
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8->display[i]){
            //If pixel is ON, draw the foreground color
            if (chip8->pixel_color[i]  != config.fg_color){
                chip8->pixel_color[i]=color_lerp(chip8->pixel_color[i], config.fg_color, config.color_lerp_rate);
            }

            const uint8_t r = (chip8->pixel_color[i] >> 24) & 0xFF; 
            const uint8_t g = (chip8->pixel_color[i] >> 16) & 0xFF; 
            const uint8_t b = (chip8->pixel_color[i] >> 8) & 0xFF; 
            const uint8_t a = (chip8->pixel_color[i] >> 0) & 0xFF; 

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            // if user requested drawing pixel outlines, draw those here. 
            if (config.pixel_outlines) {
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderDrawRect(sdl.renderer, &rect);
            }

        } else {

            if (chip8->pixel_color[i]  != config.bg_color){
                chip8->pixel_color[i]=color_lerp(chip8->pixel_color[i], config.bg_color, config.color_lerp_rate);
            }

            const uint8_t r = (chip8->pixel_color[i] >> 24) & 0xFF; 
            const uint8_t g = (chip8->pixel_color[i] >> 16) & 0xFF; 
            const uint8_t b = (chip8->pixel_color[i] >>  8) & 0xFF; 
            const uint8_t a = (chip8->pixel_color[i] >>  0) & 0xFF;

            //If pixel is OFF, draw the background color
            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }

    }
    SDL_RenderPresent(sdl.renderer);
}

//Chip8 Keypad QWERTY
//123C     1234
//456D     qwer
//789E     asdf
//A0BF     zxcv
//Original keypad format


void handle_input(chip8_t *chip8, config_t *config){
    SDL_Event event; 

    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT: 
                //Exit Window: End Program 
                chip8 -> state = QUIT;
                break; 
            
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                    case SDLK_ESCAPE: 
                        // Escape Key: Exit Window and End program
                        chip8->state = QUIT;
                        break; 

                    case SDLK_SPACE: 
                        //Space bar to pause emulation to allow the debug. 
                        if (chip8->state == RUNNING){
                            chip8->state = PAUSED;
                            puts("=======PAUSED=======");
                        } 
                        else {
                            chip8->state = RUNNING; 
                        }
                        break; 
                    case SDLK_EQUALS: 
                        // == resets CHIP8 machine for current ROM. 
                        init_chip8(chip8, config, chip8->rom_name);
                        break;

                    case SDLK_j:
                        // "j": Decrease color lerp rate
                        if (config->color_lerp_rate > 0.1){
                            config->color_lerp_rate -= 0.1;
                        }

                        break;

                    case SDLK_k:
                        // "k": Increase color lerp rate
                        if (config->color_lerp_rate < 1.0){
                            config->color_lerp_rate += 0.1;
                        }

                        break;

                    case SDLK_o:
                        // "o": Decrease volume
                        if (config->volume > 0){
                            config->volume -= 500;
                        }

                        break;

                    case SDLK_p:
                        // "p": Increase volume
                        if (config->volume < INT16_MAX){
                            config->volume += 500;
                        }
                        
                        break;
                    // Map Qwerty keys to keypad. 
                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;

                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;

                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;

                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;
                        
                    default: 
                        break; 
                }
                break; 

            case SDL_KEYUP:
                switch(event.key.keysym.sym) {

                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;

                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;

                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;

                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;
                        
                    default: 
                        break; 
                }
                break;

            default:
                break; 
        }
    }
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8, const config_t config){
    printf("Address: 0x%04X, Opcode: 0x%04X Desc: ", 
            chip8->PC -2, chip8->inst.opcode);

   switch ((chip8->inst.opcode >> 12) & 0x0F){ //Why we do this again?
        case 0x00: 
            if (chip8->inst.NN == 0xE0){
                //0x00E0: Clear the Screen
                printf("Clear Screen\n");
            }
            else if (chip8->inst.NN == 0xEE){
                //0x00EE: Return from subroutine
                //Set program counter to the last address from the subroutine stack. 
                printf("Return from subroutine to address 0x%04X\n", *(chip8->stack_ptr -1));

            } else {
                printf("Unimplemented Opcode. \n");
            }
            break; 
        case 0x01: {
            //0x1NNN: Jump to Address NNN
            printf("Jump to address NNN (0x%04X) \n", chip8->inst.NNN);
             // Set program counter to jump address
            break;
        }

        case 0x02: 
            // Assumes no invalid opcode 
            // 0x2NNN: Call subroutine at NNN
            printf("Call subroutine at NNN (0x%04X)\n",
                   chip8->inst.NNN);
            //*chip8->stack_ptr++ = chip8->PC; // Store current address to return to on subroutine stack
            //chip8->PC = chip8->inst.NNN;    // Set program counter to subroutine address so for the next instruction. 

            break;
        case 0x03: {
            //0x3XNN: Checking if VX = NN and skips to the next instruction if true. This is for branching instructions. 
            //This is like the compare instruction and jumps to something if its not equal. BNE thing. 
            printf("Check if V%X  (0x%02X) == NN (0x%02X), skip next instruction if true .\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            //chip8->PC += 2; // Skip next opcode/instruction.

            break;
        }

        case 0x04: {
            //0x4XNN: Check if VX != NN, if so, skip the next instruction.
            printf("Check if V%X  (0x%02X) != NN (0x%02X), skip next instruction if true .\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);

            break;
        }

        case 0x05: {
            //0x5XY0: Check if VX == VY and Skips the next instruction if true
           
            printf("Check if V%X  (0x%02X) == V%X (0x%02X), skip next instruction if true .\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
            
            break;
        }

        case 0x06: 
            //0x6XNN: Set VX to NN
            printf("Set Register V%X to NN (0x%02X)\n",
                   chip8->inst.X, chip8->inst.NN);
            //chip8->V[chip8->inst.X] = chip8->inst.NN; 
            break; 

        case 0x07: {
            //0x7XNN: Set VX += NN

            printf("Set Register V%X (0x%02X) += NN (0x%02X) . Result: 0x%02X \n",
                   chip8->inst.X, chip8->V[chip8->inst.X],chip8->inst.NN, chip8->V[chip8->inst.X] + chip8->inst.NN);
            
            break; 
        }

        case 0x08: {
            switch(chip8->inst.N){
                case 0: 
                    //0x8XY0: Set register VX = VY
                    printf("Set Register V%X = V%X (0x%02X) \n",
                    chip8->inst.X, chip8->inst.Y ,chip8->V[chip8->inst.Y]);

                    break; 
                case 1: 
                    //0x8XY1: Set VX |= VY
                    printf("Set Register V%X (0x%02X) |= V%X (0x%02X). Result: 0x%02X \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y ,chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);

                    break; 

                case 2: 
                    //0x8XY2: Set VX &= VY

                    printf("Set Register V%X (0x%02X) &= V%X (0x%02X). Result: 0x%02X \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y ,chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);

                    break;
                case 3: 
                    //0x8XY3: Set VX ^= VY
                    printf("Set Register V%X (0x%02X) ^= V%X (0x%02X). Result: 0x%02X \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y ,chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);

                    break;
                case 4: 
                    //0x8XY4: Set VX += VY, set VF to 1 if carry
                    printf("Set Register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y ,chip8->V[chip8->inst.Y], 
                    chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                    ((uint16_t)chip8->V[chip8->inst.X] + (uint16_t)chip8->V[chip8->inst.Y])> 255);

                    break;
                case 5: 
                    //0x8XY5: Set VX -= VY, set VF to 1 if there is not a borrow (result is positive)
                   printf("Set Register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->inst.Y ,chip8->V[chip8->inst.Y], 
                    chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                    chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]);

                    break;
                case 6: 
                    //0x8XY6: Set register VX >>=1, store shifted off bit in VF
                    printf("Set Register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], 
                    chip8->V[chip8->inst.X] & 1, 
                    chip8->V[chip8->inst.X] >> 1
                    );

                    break;

                case 7: 
                    //Ox8XY7: Set Register VX = VY -VX, set VF to 1 if theres not a borrow.
                    printf("Set Register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                    chip8->inst.X, 
                    chip8->inst.Y ,chip8->V[chip8->inst.Y], 
                    chip8->inst.X, chip8->V[chip8->inst.X],
                    chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
                    chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);

                    break;
                case 0xE:
                    //0x8XYE: Set register VX <<=1, store shifted off bit in VF
                    printf("Set Register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X \n",
                        chip8->inst.X, chip8->V[chip8->inst.X], 
                        (chip8->V[chip8->inst.X] & 0x80) >> 7, 
                        chip8->V[chip8->inst.X] << 1
                    );

                    break; 
                default:
                    break;
            }
            
            break;
        }
        case 0x09: {
            //0x9XY0: Check if VX != VY, if so, skip the next instruction.
            printf("Check if V%X  (0x%02X) != V%X (0x%02X), skip next instruction if true .\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y ,chip8->V[chip8->inst.Y]);

            break;
        }

        case 0x0A: 
            //0xANNN: Set Index Register I to NNN
            printf("Set I to NNN (0x%04X)\n",
            chip8->inst.NNN);
            break; 
        
        case 0x0B: {
            //0xBNNN: Jump to V0 + NNN
            printf("Set PC to V0 (0x%02X) + NNN (0x%04X); Result PC = 0x%04X \n", 
            chip8->V[0], chip8->inst.NNN, chip8->V[0] + chip8->inst.NNN); 
            break; 
        }
        case 0x0C: {
            //0xCXNN: Sets register VX = rand () % 256 & NN (bitwise  AND)
            printf("Set V%X = rand() % 256 & NN (0x%02X) \n", 
            chip8->inst.X, chip8->inst.NN); 
            break;
        }
        case 0x0D: 
            //0xDXYN: 
            printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) from memory location I (0x%04X)"
            "Set VF = 1 if any pixels are turned off. \n", 
            chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);

            break; 
        case 0x0E: {
            if (chip8->inst.NN == 0x9E){
                //0xEX9E: Skip the next instruction if the key stored in VX is pressed. 
                printf("Skip next instruction if key in V%X (0x%02X) is pressed; Keypad Value: %d \n", 
                        chip8->inst.X,chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]] );  

            } else if (chip8->inst.NN == 0xA1){
                //0xEXA1: Skip the next instruction if the key stored in VX is not pressed.
               printf("Skip next instruction if key in V%X (0x%02X) is not pressed; Keypad Value: %d \n", 
                        chip8->inst.X,chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]] );
            
            }

            break; 
        }
        
        case 0x0F: {
            switch(chip8->inst.NN){

                case 0x0A: 
                   //0xFX0A:  VX = get_key(); Await until a key press and store in VX
                    printf("Await unti a key is pressed; Store key in V%X \n", chip8->inst.X);
                    break;
                case 0x1E: {
                    //0xFX1E: I += VX; add VX to register I for non-Amiga CHIP8, does not effect VF
                    printf("I (0x%04X) += V%X (0x%02X); Result: (I: 0x%04X) \n", 
                                chip8->I, chip8->inst.X, chip8->V[chip8->inst.X], chip8->I + chip8->V[chip8->inst.X]);

                    break;
                }

                case 0x07: 
                    //0xFX07: VX = delay timer 
                    printf("Set V%X = delay timer value (0x%02X) \n",
                    chip8->inst.X, chip8->delay_timer);
                    
                    break;

                case 0x15: 
                    //0xFX15: delay timer = VX

                    printf("Set delay timer value = V%X (0x%02X) \n",
                    chip8->inst.X, chip8->V[chip8->inst.X]);

                    break;

                case 0x18: 
                    //0xFX18: sound timer = VX
                    printf("Set sound timer value = V%X (0x%02X) \n",
                    chip8->inst.X, chip8->V[chip8->inst.X]);
                    
                    break;
                
                case 0x29: 
                    //0xFX29: set register I to sprite location in memory for character in VX (0x0 - 0xF)
                    printf("Set I to sprite location in memory for character V%X (0x%02X). Result (VX*5) = 0x%02X \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);

                    break;

                case 0x33: {
                    // 0xFX33: Store BCD Representation of VX at memory offset from I; 
                    // I = hundreds place, I + 1 = ten's place, I + 2 = one's place 
                    printf("Store BCD representation of V%X (0x%02X) at memory from I (0x%04X) \n",
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);

                    break;
                }

                case 0x55: {
                    //0xFX55: Register dump V0 - VX inclusive to memory offset from I; 
                    //SCHIP does not increment I, chip8 does increment I

                    printf("Register dump V0 - V%X (0x%02X) inclusive at memory from I (0x%04X) \n", 
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);

                    break;
                }

                case 0x65: 
                    //0xFX65: Register Load V0-VX inclusive from memory offset from I; 
                    
                    printf("Register load V0 - V%X (0x%02X) inclusive at memory from I (0x%04X) \n", 
                    chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);

                    break;

                default:
                    break;
            }
            break; 
        }

        default: 
            printf("Unimplemented Opcode. \n");
            break; 
    } 
}
#endif

// Emulate 1 instruction. 
void emulate_instruction(chip8_t *chip8, const config_t config){

    //Defining the Carry Flag
    bool carry;
    // Get next opcode from RAM
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC +1]; //2 bytes big endian architecture. 
    chip8->PC += 2; // Pre-increment program counter for next opcode. 

    //What's the reality of the or-ing of the values? why would you do it anyway? This is something that 
    //Needs to be answered asap. 
    //Fill Out instruction format: 
    //DXYN
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; 
    chip8->inst.NN = chip8->inst.opcode & 0x00FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F; 
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F; 

#ifdef DEBUG 
    print_debug_info(chip8, config);
#endif


    //Emulate the Opcode (Only 35 in the whole thing)
    switch ((chip8->inst.opcode >> 12) & 0x0F){ //Why we do this again?
        case 0x00: 
           { if (chip8->inst.NN == 0xE0){
                //0x00E0: Clear the Screen
                memset(&chip8->display[0], false, sizeof(chip8->display));
                chip8->draw = true;
            }
            else if (chip8->inst.NN == 0xEE){
                //0x00EE: Return from subroutine
                //Set program counter to the last address from the subroutine stack. 

                chip8->PC = *(--chip8->stack_ptr);

            } else {

            }

            break; 
           }

        case 0x01: {
            //0x1NNN: Jump to Address NNN
            chip8->PC = chip8->inst.NNN;  // Set program counter to jump address
            break;
        }

        case 0x02: {
            // Assumes no invalid opcode 
            // 0x2NNN: Call subroutine at NNN
            *chip8->stack_ptr++ = chip8->PC; // Store current address to return to on subroutine stack
            chip8->PC = chip8->inst.NNN;    // Set program counter to subroutine address so for the next instruction. 
            break;
        }

        case 0x03: {
            //0x3XNN: Checking if VX = NN and skips to the next instruction if true. This is for branching instructions. 
            //This is like the compare instruction and jumps to something if its not equal. BNE thing. 
            
            if (chip8->V[chip8->inst.X] == chip8->inst.NN){
                chip8->PC += 2; // Skip next opcode/instruction.
            }

            break;
        }

        case 0x04: {
            //0x4XNN: Check if VX != NN, if so, skip the next instruction.

            if (chip8->V[chip8->inst.X] != chip8->inst.NN){
                chip8->PC += 2; // Skip next opcode/instruction.
            }

            break;
        }

        case 0x05: {
            //0x5XY0: Check if VX == VY and Skips the next instruction if true
            if (chip8->inst.N != 0){
                break; //Wrong opcode
            }
            
            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) {
                chip8->PC +=2; 
            }
            
            break;
        }

        case 0x06: {
            //0x6XNN: Set VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN; 
            break; 

        }

        case 0x07: {
            //0x7XNN: Set VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN; 
            break; 

        }

        case 0x08: {
            switch(chip8->inst.N){
                case 0: 
                    //0x8XY0: Set register VX = VY
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    
                    break; 

                case 1: 
                    //0x8XY1: Set VX |= VY
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    //CHIP8 only quirk
                    if (config.current_extension == CHIP8){
                        chip8->V[0xF] = 0;
                    }
                    break;

                case 2: 
                    //0x8XY2: Set VX &= VY
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    //CHIP8 only quirk
                    if (config.current_extension == CHIP8){
                        chip8->V[0xF] = 0;
                    }
                    break;

                case 3: 
                    //0x8XY3: Set VX ^= VY
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    //CHIP8 only quirk
                    if (config.current_extension == CHIP8){
                        chip8->V[0xF] = 0;
                    }
                    break;

                case 4: {
                    //0x8XY4: Set VX += VY, set VF to 1 if carry
                    carry = (((uint16_t)chip8->V[chip8->inst.X] + (uint16_t)chip8->V[chip8->inst.Y]) > 255);
                    
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;
                } 
                case 5: 
                    //0x8XY5: Set VX -= VY, set VF to 1 if there is not a borrow (result is positive)
                    carry = ((chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
                    
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];

                    chip8->V[0xF] = carry;

                    break;
                case 6: 
                    //0x8XY6: Set register VX >>=1, store shifted off bit in VF
                    if (config.current_extension == CHIP8){
                        carry = chip8->V[chip8->inst.Y] & 1;
                        chip8->V[chip8->inst.Y] >> 1;
                        chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;
                    } else {
                        carry = chip8->V[chip8->inst.X] & 1;
                        chip8->V[chip8->inst.X] >>= 1; 
                    }
                    

                    chip8->V[0xF] = carry;

                    break;

                case 7: 
                    //Ox8XY7: Set Register VX = VY -VX, set VF to 1 if theres not a borrow.
                    carry = ((chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
                    
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];

                    chip8->V[0xF] = carry;

                    break;

                case 0xE:
                    //0x8XYE: Set register VX <<=1, store shifted off bit in VF

                    if (config.current_extension == CHIP8){
                        carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;
                        chip8->V[chip8->inst.X] =  chip8->V[chip8->inst.Y] << 1;
                    } else {
                        carry = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                        chip8->V[chip8->inst.X] <<= 1; 
                    }
                    
                    chip8->V[0xF] = carry;
                    break; 
                default:
                    break;
            }
            
            break;
        }
        case 0x09: {
            //0x9XY0: Check if VX != VY, skip the next instruction if true. 
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){
                chip8->PC += 2; // Skip next opcode/instruction.
            }

            break;
        }
        case 0x0A: {
            //0xANNN: Set Index Register I to NNN
            chip8->I = chip8->inst.NNN;
            break; 
        }

        case 0x0B: {
            //0xBNNN: Jump to V0 + NNN

            chip8->PC = chip8->V[0] + chip8->inst.NNN; 
            break; 
        }

        case 0x0C: {
            //0xCXNN: Sets register VX = rand () % 256 & NN (bitwise  AND)

            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN; 
            break;
        }

        case 0x0D: {
            //0xDXYN: Draw N height sprite at coordinates X, Y: Read from memory location I: 
            // Screen pixels are XOR'd with sprite bits
            // VF (Carry Flag) is set if any screen pixels are set off.
            
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            
            const uint8_t orig_X = X_coord; 


            chip8->V[0xF] = 0; //Initialize carry flag to 0. 

            // Loop over all N rows of the sprite. 
            for (uint8_t i = 0; i< chip8->inst.N; i++){
                //Get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X; //Reset X for next row drawing. 

                for (int8_t j = 7; j >= 0; j--){
                    //If sprite pixel/bit is on and display pixel is on, set carry flag
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];
                    const bool sprite_bit = (sprite_data & (1 << j));

                    /*
                    Previous Version of Code: 
                    if ((sprite_data & (1 << j)) && *pixel ) {
                        chip8->V[0xF] = 1; 
                    }
                    */
                    
                    if (sprite_bit && *pixel ) {
                        chip8->V[0xF] = 1; 
                    }

                    //Why did doing it with sprite bit instead of doing it with the data itself change the value?
                    //Like, instead of sprite bit itself, when I was doing it 
                    //where the sprite bit variable was replaced by the value calculation. 
                    //That happened twice. Does that affect things.
                
                    /*
                    Previous Version of Code: 
                     *pixel ^= sprite_data & (1 << j);

                    */

                    //XOR display pixel with sprite pixel/bit
                    *pixel ^= sprite_bit;

                    //Stop drawing if hit right edge of screen

                    if( ++X_coord >= config.window_width){
                        break;
                    }
                }

                // Stop drawing entire sprite if at bottom edge of screen. 
                if (++Y_coord >= config.window_height) break;
            }
            chip8->draw = true;
            break; 
        }
        case 0x0E: {
            if (chip8->inst.NN == 0x9E){
                //0xEX9E: Skip the next instruction if the key stored in VX is pressed. 
                if (chip8->keypad[chip8->V[chip8->inst.X]]) {
                    chip8->PC += 2;
                }
                 

            } else if (chip8->inst.NN == 0xA1){
                //0xEXA1: Skip the next instruction if the key stored in VX is not pressed.
                if (!chip8->keypad[chip8->V[chip8->inst.X]]) {
                    chip8->PC += 2;
                }
            
            }

            break; 
        }
        case 0x0F: {
            switch(chip8->inst.NN){
                case 0x0A: {
                    // 0xFX0A: VX = get_key(); Await until a keypress, and store in VX
                    static bool any_key_pressed = false;
                    static uint8_t key = 0xFF;

                    for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypad; i++) 
                        if (chip8->keypad[i]) {
                            key = i;    // Save pressed key to check until it is released
                            any_key_pressed = true;
                            break;
                        }

                    // If no key has been pressed yet, keep getting the current opcode & running this instruction
                    if (!any_key_pressed) chip8->PC -= 2; 
                    else {
                        // A key has been pressed, also wait until it is released to set the key in VX
                        if (chip8->keypad[key])     // "Busy loop" CHIP8 emulation until key is released
                            chip8->PC -= 2;
                        else {
                            chip8->V[chip8->inst.X] = key;     // VX = key 
                            key = 0xFF;                        // Reset key to not found 
                            any_key_pressed = false;           // Reset to nothing pressed yet
                        }
                    }
                    break;
                }
                
                case 0x1E: {
                    //0xFX1E: I += VX; add VX to register I for non-Amiga CHIP8, does not effect VF

                    chip8->I += chip8->V[chip8->inst.X];

                    break;
                }

                case 0x07: 
                    //0xFX07: VX = delay timer 

                    chip8->V[chip8->inst.X] = chip8->delay_timer;

                    break;

                case 0x15: 
                    //0xFX15: delay timer = VX

                    chip8->delay_timer = chip8->V[chip8->inst.X] ;

                    break;

                case 0x18: 
                    //0xFX18: sound timer = VX

                    chip8->sound_timer = chip8->V[chip8->inst.X] ;

                    break;
                
                case 0x29: 
                    //0xFX29: set register I to sprite location in memory for character in VX (0x0 - 0xF)
                    chip8->I = chip8->V[chip8->inst.X] * 5;

                    break;

                case 0x33: {
                    // 0xFX33: Store BCD Representation of VX at memory offset from I; 
                    // I = hundreds place, I + 1 = ten's place, I + 2 = one's place 

                    uint8_t bcd = chip8->V[chip8->inst.X];

                    chip8->ram[chip8->I+2] = bcd % 10;
                    bcd /= 10; 

                    chip8->ram[chip8->I+1] = bcd % 10; 
                    bcd /= 10; 

                    chip8->ram[chip8->I] = bcd; 
                    
                    break;
                }

                case 0x55: {
                    //0xFX55: Register dump V0 - VX inclusive to memory offset from I; 
                    //SCHIP does not increment I, chip8 does increment I
                    //Note could make this a config flag to use Super Chip or Chip8 logic for I (essentially whether we would want to change the flag or not.)

                    for (uint8_t i = 0; i <= chip8->inst.X; i++){
                        if (config.current_extension == CHIP8){
                            chip8->ram[chip8->I++] = chip8->V[i];
                            //I++ Increment I each time. 
                        } else {
                            chip8->ram[chip8->I + i] = chip8->V[i]; }
                    }

                    break;
                }

                case 0x65: {
                    //0xFX65: Register Load V0-VX inclusive from memory offset from I; 
                    
                    for (uint8_t i = 0; i <= chip8->inst.X; i++){
                        if (config.current_extension == CHIP8){
                            chip8->V[i] = chip8->ram[chip8->I++];
                        } else {
                            chip8->V[i] = chip8->ram[chip8->I + i];
                        }
                         
                    }
                    
                    break;
                }

                default:
                    break;
            }
            break; 
        }
        default: 
            break; 
    } 

}

// Update CHIP8 delay and sound timers every 60 Hz
void update_timers( const sdl_t sdl, chip8_t *chip8){
    if(chip8->delay_timer > 0){
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0 ){
        chip8->sound_timer--; 
        // TODO: Play Sound
        SDL_PauseAudioDevice(sdl.dev, 0);
    }
    else {
        // TODO: Stop playing the sound
        SDL_PauseAudioDevice(sdl.dev, 1);
    }
}

int hit_count = 0; 

int main(int argc, char **argv){
    //Default Usage Message for args
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_name> \n", argv[0]);
        exit(EXIT_FAILURE);
    }


    config_t config = {0}; 
    if (!set_config(&config, argc, argv)){
        exit(EXIT_FAILURE);
    }

    //Initialize SDL 
    sdl_t sdl = {0}; 
    if(!initialize_SDL(&sdl, &config)){
        exit(EXIT_FAILURE);
    }

    //INITIALIZE THE CHIP 

    chip8_t chip8 = {QUIT};

    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, &config, rom_name)){
        exit(EXIT_FAILURE);
    }

    // Initial Screen Clear 
    clear_screen(sdl, config);

    //srand(time(NULL));
    srand(10);
    //Main Emulator Loop

    while (chip8.state != QUIT){
        //Handle User Inputs
        handle_input(&chip8, &config);

        if (chip8.state == PAUSED) {
            continue;
        }
        //Get_time() before running instrucions;

        const uint64_t start_frame_time = SDL_GetPerformanceCounter();

        //Emulate Chip8 Instructions for this emulator frame.
        for (uint32_t i = 0; i < config.inst_per_second / 60; i++){
            emulate_instruction(&chip8, config);
        }
        
        //Get time elapsed after running instruction
        const uint64_t end_frame_time = SDL_GetPerformanceCounter();

        //Get_time() elapsed since last get_time()

        //Delay for approx 60 fps
        const double time_elapsed = (double)((end_frame_time - start_frame_time) * 1000)/ SDL_GetPerformanceFrequency();

        //SDL_Delay(16 - actual_time_elapsed);

        SDL_Delay(16.67f> time_elapsed ? 16.67f - time_elapsed : 0);

        //Update window with changes 

        if (chip8.draw) {
            Update_Screen(sdl, &chip8, config);
            chip8.draw = false; 
        }
        

        hit_count += 1; 

        //update delay and sound timers every 60 Hz
        update_timers(sdl, &chip8);
    }

    /*
    //Testing Code: 

    while(true){
       
        printf("PRINTING SDL");
    }

    */

    //Final Cleanup
    final_cleanup(sdl); 
    
    exit(EXIT_SUCCESS);
    return 0; 

}