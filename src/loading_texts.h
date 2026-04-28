#ifndef LOADING_TEXTS_H
#define LOADING_TEXTS_H

#define NUM_LOADING_TEXTS 25

#define HISTORY_SIZE 20

static int history[HISTORY_SIZE] = { -1 };
static int history_index = 0;



bool is_recent(int idx) {
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (history[i] == idx) return true;
    }
    return false;
}


void add_to_history(int idx) {
    history[history_index] = idx;
    history_index = (history_index + 1) % HISTORY_SIZE;
}



static const char *loading_texts[NUM_LOADING_TEXTS] = {
    "Counting pixels...",
    "Untangling cables...",
    "Booting technician...",
    "Polishing light beams...",
    "Adjusting faders...",
    "Filling Co2...",
    "Loading confetti...",
    "Calibrating laser beams...",
    "Warming up fog machine...",
    "Chilling CO2......",
    "Mic check... one.. two",
    "Buffering applause...",
    "Searching for the missing adapter...",
    "Initializing caffeine protocol...",
    "Backing up the backup showfile...",
    "Applying Gaffer...",
    "Loading WOOSHHH...",
    "Reloading confetti...",
    "Explosions warming up...",
    "Activate the awesome...",
    "Deploying sparkle...",
    "Loading tension...",
    "Activating kaboom...",
    "Just a little sparkle...",
   "Arming safety channels..."
};

#endif // LOADING_TEXTS_H
