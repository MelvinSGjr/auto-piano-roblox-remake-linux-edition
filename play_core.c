#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <stdatomic.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

atomic_bool isPlaying = false;
atomic_bool legitModeActive = false;
atomic_int storedIndex = 0;
atomic_double elapsedTime = 0;
double origionalPlaybackSpeed = 1.0;
double speedMultiplier = 2.0;
double playback_speed = 1.0;

typedef struct {
    double delay;
    char* notes;
} NoteInfo;

typedef struct {
    double tempo;
    double tOffset;
    NoteInfo* notes;
    size_t notes_count;
} SongInfo;

SongInfo* infoTuple = NULL;

typedef struct {
    char key;
    double hold_until;
} HeldNote;

HeldNote* heldNotes = NULL;
size_t heldNotes_count = 0;
size_t heldNotes_capacity = 0;

Display* display = NULL;

void init_keyboard() {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
}

void press_key(KeySym keysym) {
    if (!display) return;
    
    KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode) {
        XTestFakeKeyEvent(display, keycode, True, 0);
        XFlush(display);
    }
}

void release_key(KeySym keysym) {
    if (!display) return;
    
    KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode) {
        XTestFakeKeyEvent(display, keycode, False, 0);
        XFlush(display);
    }
}

void press_letter(char strLetter) {
    KeySym keysym;
    
    if (isupper(strLetter) || ispunct(strLetter)) {
        keysym = XStringToKeysym(&strLetter);
        if (keysym == NoSymbol) {
            if (strLetter == '!') keysym = XK_exclam;
            else if (strLetter == '@') keysym = XK_at;
            else if (strLetter == '#') keysym = XK_numbersign;
            else if (strLetter == '$') keysym = XK_dollar;
            else if (strLetter == '%') keysym = XK_percent;
            else if (strLetter == '^') keysym = XK_asciicircum;
            else if (strLetter == '&') keysym = XK_ampersand;
            else if (strLetter == '*') keysym = XK_asterisk;
            else if (strLetter == '(') keysym = XK_parenleft;
            else if (strLetter == ')') keysym = XK_parenright;
            else keysym = XStringToKeysym(&strLetter);
        }
        
        press_key(XK_Shift_L);
        press_key(keysym);
        release_key(keysym);
        release_key(XK_Shift_L);
    } else {
        keysym = XStringToKeysym(&strLetter);
        if (keysym == NoSymbol) {
            if (isdigit(strLetter)) {
                keysym = XK_0 + (strLetter - '0');
            } else {
                keysym = XK_a + (tolower(strLetter) - 'a');
            }
        }
        
        press_key(keysym);
        release_key(keysym);
    }
}

void release_letter(char strLetter) {
    KeySym keysym;
    
    if (isupper(strLetter) || ispunct(strLetter)) {
        keysym = XStringToKeysym(&strLetter);
        if (keysym == NoSymbol) {
            if (strLetter == '!') keysym = XK_exclam;
            else if (strLetter == '@') keysym = XK_at;
            else if (strLetter == '#') keysym = XK_numbersign;
            else if (strLetter == '$') keysym = XK_dollar;
            else if (strLetter == '%') keysym = XK_percent;
            else if (strLetter == '^') keysym = XK_asciicircum;
            else if (strLetter == '&') keysym = XK_ampersand;
            else if (strLetter == '*') keysym = XK_asterisk;
            else if (strLetter == '(') keysym = XK_parenleft;
            else if (strLetter == ')') keysym = XK_parenright;
            else keysym = XStringToKeysym(&strLetter);
        }
        
        release_key(keysym);
    } else {
        keysym = XStringToKeysym(&strLetter);
        if (keysym == NoSymbol) {
            if (isdigit(strLetter)) {
                keysym = XK_0 + (strLetter - '0');
            } else {
                keysym = XK_a + (tolower(strLetter) - 'a');
            }
        }
        
        release_key(keysym);
    }
}

double calculateTotalDuration(NoteInfo* notes, size_t count) {
    double total = 0;
    for (size_t i = 0; i < count; i++) {
        total += notes[i].delay;
    }
    return total;
}

int isShifted(char charIn) {
    if (isupper(charIn)) return 1;
    if (ispunct(charIn)) return 1;
    return 0;
}

void toggleLegitMode() {
    legitModeActive = !legitModeActive;
    printf("Legit Mode turned %s\n", legitModeActive ? "ON" : "OFF");
}

void speedUp() {
    playback_speed *= speedMultiplier;
    printf("Speeding up: Playback speed is now %.2fx\n", playback_speed);
}

void slowDown() {
    playback_speed /= speedMultiplier;
    printf("Slowing down: Playback speed is now %.2fx\n", playback_speed);
}

double floorToZero(double i) {
    return i > 0 ? i : 0;
}

SongInfo* processFile() {
    FILE* file = fopen("song.txt", "r");
    if (!file) {
        printf("Couldn't open song.txt\n");
        return NULL;
    }
    
    SongInfo* song = malloc(sizeof(SongInfo));
    if (!song) {
        fclose(file);
        return NULL;
    }
    
    song->tempo = 0;
    song->tOffset = 0;
    song->notes = NULL;
    song->notes_count = 0;
    
    char line[256];
    int tOffsetSet = 0;
    size_t notes_capacity = 0;
    
    if (fgets(line, sizeof(line), file)) {
        if (strstr(line, "playback_speed=")) {
            playback_speed = atof(line + 15);
            printf("Playback speed is set to %.2fx\n", playback_speed);
        } else {
            printf("First line should be playback_speed=1.0\n");
            fclose(file);
            free(song);
            return NULL;
        }
    }
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strstr(line, "tempo=")) {
            song->tempo = 60.0 / atof(line + 6);
        } else {
            char* space = strchr(line, ' ');
            if (!space) continue;
            
            *space = 0;
            double waitToPress = atof(line);
            char* notes = space + 1;
            
            if (!tOffsetSet) {
                song->tOffset = waitToPress;
                tOffsetSet = 1;
            }
            
            if (song->notes_count >= notes_capacity) {
                notes_capacity = notes_capacity ? notes_capacity * 2 : 16;
                song->notes = realloc(song->notes, sizeof(NoteInfo) * notes_capacity);
                if (!song->notes) {
                    fclose(file);
                    free(song);
                    return NULL;
                }
            }
            
            song->notes[song->notes_count].delay = waitToPress;
            song->notes[song->notes_count].notes = strdup(notes);
            song->notes_count++;
        }
    }
    
    fclose(file);
    
    if (song->tempo == 0) {
        printf("No tempo found\n");
        free(song->notes);
        free(song);
        return NULL;
    }
    
    return song;
}

NoteInfo* parseInfo(SongInfo* song) {
    if (!song || song->notes_count == 0) {
        printf("No notes to parse\n");
        return NULL;
    }
    
    NoteInfo* notes = malloc(sizeof(NoteInfo) * song->notes_count);
    if (!notes) return NULL;
    
    memcpy(notes, song->notes, sizeof(NoteInfo) * song->notes_count);
    size_t notes_count = song->notes_count;
    
    double tempo = song->tempo;
    size_t i = 0;
    
    while (i < notes_count - 1) {
        if (strstr(notes[i].notes, "tempo=")) {
            tempo = 60.0 / atof(notes[i].notes + 6);
            free(notes[i].notes);
            for (size_t j = i; j < notes_count - 1; j++) {
                notes[j] = notes[j + 1];
            }
            notes_count--;
        } else {
            notes[i].delay = (notes[i + 1].delay - notes[i].delay) * tempo;
            i++;
        }
    }
    
    if (notes_count > 0) {
        notes[notes_count - 1].delay = 1.00;
    }
    
    NoteInfo* result = realloc(notes, sizeof(NoteInfo) * notes_count);
    if (!result) {
        free(notes);
        return NULL;
    }
    
    return result;
}

void adjustTempoForCurrentNote() {
}

void releaseHeldNotes(const char* note_keys) {
    for (size_t i = 0; i < strlen(note_keys); i++) {
        for (size_t j = 0; j < heldNotes_count; j++) {
            if (heldNotes[j].key == note_keys[i]) {
                release_letter(note_keys[i]);
                heldNotes[j].key = '\0';
                break;
            }
        }
    }
    
    size_t new_count = 0;
    for (size_t i = 0; i < heldNotes_count; i++) {
        if (heldNotes[i].key != '\0') {
            heldNotes[new_count++] = heldNotes[i];
        }
    }
    heldNotes_count = new_count;
}

double calculate_note_complexity(const char* notes) {
    int count = strlen(notes);
    double complexity = count * 1.5;
    
    if (count > 3) complexity += (count - 3) * 0.7;
    
    for (int i = 0; i < count; i++) {
        if (isupper(notes[i]) || ispunct(notes[i])) {
            complexity += 0.5;
        }
    }
    
    return complexity;
}

void* playNextNote(void* arg) {
    if (!isPlaying || !infoTuple || storedIndex >= infoTuple->notes_count) {
        isPlaying = false;
        storedIndex = 0;
        elapsedTime = 0;
        return NULL;
    }
    
    adjustTempoForCurrentNote();
    
    NoteInfo noteInfo = infoTuple->notes[storedIndex];
    double delay = floorToZero(noteInfo.delay);
    const char* note_keys = noteInfo.notes;
    
    double total_duration = calculateTotalDuration(infoTuple->notes, infoTuple->notes_count);
    
    static double humanization_factor = 1.0;
    static int error_count = 0;
    static double timing_accuracy = 0.97;
    
    if (legitModeActive) {
        double complexity = calculate_note_complexity(note_keys);
        
        double human_delay = delay;
        
        if (complexity > 3.0) {
            human_delay *= (0.95 + (rand() % 11) / 100.0);
        }
        
        if (strlen(note_keys) > 1) {
            double chord_spread = complexity * 0.005 + (rand() % 10) / 1000.0;
            human_delay += chord_spread;
        }
        
        double timing_variation = (rand() % 21 - 10) / 100.0;
        human_delay *= (1.0 + timing_variation);
        
        if (complexity > 4.0 && rand() % 100 < 15) {
            human_delay *= (0.8 + (rand() % 15) / 100.0);
        }
        
        if (rand() % 200 < 5 && error_count < 2) {
            human_delay *= 1.2;
            error_count++;
        }
        
        if (rand() % 300 < 3 && complexity < 3.0) {
            human_delay = 0;
        }
        
        delay = human_delay;
        
        if (rand() % 500 < 2) {
            timing_accuracy -= 0.02;
            if (timing_accuracy < 0.7) timing_accuracy = 0.7;
        }
        
        if (rand() % 400 < 3) {
            timing_accuracy += 0.03;
            if (timing_accuracy > 1.05) timing_accuracy = 1.05;
        }
        
        delay *= timing_accuracy;
        
        if (rand() % 1000 < 2) {
            humanization_factor = 0.7 + (rand() % 6) / 10.0;
        }
        
        if (rand() % 800 < 3) {
            humanization_factor = 1.0;
        }
        
        delay *= humanization_factor;
    }
    
    elapsedTime += delay > 0 ? delay : 0;
    
    if (strchr(note_keys, '~')) {
        releaseHeldNotes(note_keys);
    } else {
        if (legitModeActive && strlen(note_keys) > 1) {
            double complexity = calculate_note_complexity(note_keys);
            double note_delay = complexity * 0.003 + (rand() % 10) / 1000.0;
            
            for (size_t i = 0; i < strlen(note_keys); i++) {
                press_letter(note_keys[i]);
                
                if (heldNotes_count >= heldNotes_capacity) {
                    heldNotes_capacity = heldNotes_capacity ? heldNotes_capacity * 2 : 16;
                    heldNotes = realloc(heldNotes, sizeof(HeldNote) * heldNotes_capacity);
                }
                
                heldNotes[heldNotes_count].key = note_keys[i];
                heldNotes[heldNotes_count].hold_until = elapsedTime + noteInfo.delay / playback_speed;
                heldNotes_count++;
                
                if (i < strlen(note_keys) - 1) {
                    usleep(note_delay * 1000000);
                }
            }
        } else {
            for (size_t i = 0; i < strlen(note_keys); i++) {
                press_letter(note_keys[i]);
                
                if (heldNotes_count >= heldNotes_capacity) {
                    heldNotes_capacity = heldNotes_capacity ? heldNotes_capacity * 2 : 16;
                    heldNotes = realloc(heldNotes, sizeof(HeldNote) * heldNotes_capacity);
                }
                
                heldNotes[heldNotes_count].key = note_keys[i];
                heldNotes[heldNotes_count].hold_until = elapsedTime + noteInfo.delay / playback_speed;
                heldNotes_count++;
            }
        }
        
        double total_mins, total_secs, elapsed_mins, elapsed_secs;
        total_mins = total_duration / 60;
        total_secs = total_duration - (int)total_mins * 60;
        elapsed_mins = elapsedTime / 60;
        elapsed_secs = elapsedTime - (int)elapsed_mins * 60;
        
        printf("[%dm %ds/%dm %ds] %s\n", 
               (int)elapsed_mins, (int)elapsed_secs,
               (int)total_mins, (int)total_secs,
               note_keys);
    }
    
    storedIndex++;
    
    if (delay <= 0) {
        playNextNote(NULL);
    } else {
        usleep(delay * 1000000 / playback_speed);
        playNextNote(NULL);
    }
    
    return NULL;
}

void onDelPress() {
    isPlaying = !isPlaying;
    
    if (isPlaying) {
        printf("Playing...\n");
        pthread_t thread;
        pthread_create(&thread, NULL, playNextNote, NULL);
        pthread_detach(thread);
    } else {
        printf("Stopping...\n");
        for (size_t i = 0; i < heldNotes_count; i++) {
            if (heldNotes[i].key != '\0') {
                release_letter(heldNotes[i].key);
            }
        }
        heldNotes_count = 0;
    }
}

void rewind() {
    if (storedIndex - 10 < 0) {
        storedIndex = 0;
    } else {
        storedIndex -= 10;
    }
    printf("Rewound to %d\n", storedIndex);
}

void skip() {
    if (storedIndex + 10 > infoTuple->notes_count) {
        isPlaying = false;
        storedIndex = 0;
    } else {
        storedIndex += 10;
    }
    printf("Skipped to %d\n", storedIndex);
}

void printControls() {
    printf("\n====================\n");
    printf("      Controls      \n");
    printf("====================\n");
    printf("DELETE   : Play/Pause\n");
    printf("HOME     : Rewind\n");
    printf("END      : Advance\n");
    printf("PAGE UP  : Speed Up\n");
    printf("PAGE DOWN: Slow Down\n");
    printf("INSERT   : Toggle Legit Mode\n");
    printf("F5       : Load New Song\n");
    printf("ESC      : Exit\n");
    printf("====================\n\n");
}

NoteInfo* simplify_notes(NoteInfo* notes, size_t count) {
    return notes;
}

int main() {
    init_keyboard();
    srand(time(NULL));
    
    infoTuple = processFile();
    if (!infoTuple) {
        printf("Can't start: song file is missing or broken\n");
        return 1;
    }
    
    NoteInfo* parsed_notes = parseInfo(infoTuple);
    if (!parsed_notes) {
        printf("No notes to play\n");
        free(infoTuple);
        return 1;
    }
    
    free(infoTuple->notes);
    infoTuple->notes = parsed_notes;
    infoTuple->notes_count = 0;
    
    while (infoTuple->notes[infoTuple->notes_count].notes != NULL) {
        infoTuple->notes_count++;
    }
    
    infoTuple->notes = simplify_notes(infoTuple->notes, infoTuple->notes_count);
    
    printControls();
    
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    Window root = DefaultRootWindow(dpy);
    XEvent ev;
    
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Delete), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Home), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_End), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Page_Up), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Page_Down), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Insert), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F5), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Escape), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    
    printf("Press ESC to exit\n");
    
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            KeySym keysym = XLookupKeysym(&ev.xkey, 0);
            
            if (keysym == XK_Delete) {
                onDelPress();
            } else if (keysym == XK_Home) {
                rewind();
            } else if (keysym == XK_End) {
                skip();
            } else if (keysym == XK_Page_Up) {
                speedUp();
            } else if (keysym == XK_Page_Down) {
                slowDown();
            } else if (keysym == XK_Insert) {
                toggleLegitMode();
            } else if (keysym == XK_F5) {
                printf("Reloading song...\n");
                isPlaying = false;
                
                for (size_t i = 0; i < infoTuple->notes_count; i++) {
                    free(infoTuple->notes[i].notes);
                }
                free(infoTuple->notes);
                free(infoTuple);
                
                infoTuple = processFile();
                if (infoTuple) {
                    NoteInfo* parsed_notes = parseInfo(infoTuple);
                    if (parsed_notes) {
                        free(infoTuple->notes);
                        infoTuple->notes = parsed_notes;
                        infoTuple->notes_count = 0;
                        
                        while (infoTuple->notes[infoTuple->notes_count].notes != NULL) {
                            infoTuple->notes_count++;
                        }
                    }
                }
            } else if (keysym == XK_Escape) {
                break;
            }
        }
    }
    
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XCloseDisplay(dpy);
    
    for (size_t i = 0; i < infoTuple->notes_count; i++) {
        free(infoTuple->notes[i].notes);
    }
    free(infoTuple->notes);
    free(infoTuple);
    free(heldNotes);
    
    if (display) {
        XCloseDisplay(display);
    }
    
    return 0;
}