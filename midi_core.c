#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define MIDI_HEADER "MThd"
#define MIDI_TRACK "MTrk"
#define MIDI_END_OF_TRACK 0xFF

uint8_t startSequence[3][4] = {
    {0x4D, 0x54, 0x68, 0x64},  // MThd
    {0x4D, 0x54, 0x72, 0x6B},  // MTrk
    {0xFF, 0x00, 0x00, 0x00}   // FF (only first byte used)
};

typedef struct {
    uint8_t code;
    const char* description;
} MetaEventType;

MetaEventType typeDict[] = {
    {0x00, "Sequence Number"},
    {0x01, "Text Event"},
    {0x02, "Copyright Notice"},
    {0x03, "Sequence/Track Name"},
    {0x04, "Instrument Name"},
    {0x05, "Lyric"},
    {0x06, "Marker"},
    {0x07, "Cue Point"},
    {0x20, "MIDI Channel Prefix"},
    {0x2F, "End of Track"},
    {0x51, "Set Tempo"},
    {0x54, "SMPTE Offset"},
    {0x58, "Time Signature"},
    {0x59, "Key Signature"},
    {0x7F, "Sequencer Specific"},
    {0x21, "Prefix Port"},
    {0x09, "Other text format [0x09]"},
    {0x08, "Other text format [0x08]"},
    {0x0A, "Other text format [0x0A]"},
    {0x0C, "Other text format [0x0C]"},
    {0, NULL}
};

typedef struct {
    double time;
    char* note;
} MidiNote;

typedef struct {
    int verbose;
    int debug;
    
    uint32_t header_length;
    uint16_t format;
    uint16_t tracks;
    uint16_t division;
    uint16_t division_type;
    
    size_t itr;
    int running_status;
    int running_status_set;
    double tempo;
    
    uint8_t* bytes;
    size_t bytes_size;
    
    char* filename;
    char* record_file;
    
    int delta_time_started;
    double delta_time;
    
    uint32_t key_press_count;
    
    char piano_scale[64];
    
    int start_counter[3];
    
    MidiNote* notes;
    size_t notes_count;
    size_t notes_capacity;
    
    char** log_entries;
    size_t log_count;
    size_t log_capacity;
    
    int success;
} MidiReader;

MidiReader* midi_reader_init(const char* filename);
void midi_reader_cleanup(MidiReader* reader);
void process_midi_file(MidiReader* reader, const char* record_file);
int check_start_sequence(MidiReader* reader);
void skip_bytes(MidiReader* reader, size_t count);
uint32_t read_variable_length(MidiReader* reader);
void read_mthd(MidiReader* reader);
void read_mtrk(MidiReader* reader);
char* read_text(MidiReader* reader, size_t length);
int read_midi_meta_event(MidiReader* reader, uint32_t deltaT);
void read_midi_track_event(MidiReader* reader, uint32_t length);
void read_voice_event(MidiReader* reader, uint32_t deltaT);
void read_events(MidiReader* reader);
void log_message(MidiReader* reader, const char* format, ...);
uint32_t get_int(MidiReader* reader, size_t count);
void clean_notes(MidiReader* reader);
void save_song(MidiReader* reader, const char* song_file);
void save_sheet(MidiReader* reader, const char* sheet_file);
void save_record(MidiReader* reader, const char* record_file);

MidiReader* midi_reader_init(const char* filename) {
    MidiReader* reader = (MidiReader*)malloc(sizeof(MidiReader));
    if (!reader) return NULL;

    reader->verbose = 0;
    reader->debug = 0;
    
    reader->header_length = 0;
    reader->format = 0;
    reader->tracks = 0;
    reader->division = 480;
    reader->division_type = 0;
    
    reader->itr = 0;
    reader->running_status = -1;
    reader->running_status_set = 0;
    reader->tempo = 0;
    
    reader->bytes = NULL;
    reader->bytes_size = 0;
    
    reader->filename = strdup(filename);
    reader->record_file = strdup("midiRecord.txt");
    
    reader->delta_time_started = 0;
    reader->delta_time = 0;
    
    reader->key_press_count = 0;
    
    strcpy(reader->piano_scale, "1!2@34$5%6^78*9(0qQwWeErtTyYuiIoOpPasSdDfgGhHjJklLzZxcCvVbBnm");
    
    memset(reader->start_counter, 0, sizeof(reader->start_counter));
    
    reader->notes = NULL;
    reader->notes_count = 0;
    reader->notes_capacity = 0;
    
    reader->log_entries = NULL;
    reader->log_count = 0;
    reader->log_capacity = 0;
    
    reader->success = 0;
    
    return reader;
}

void midi_reader_cleanup(MidiReader* reader) {
    if (!reader) return;
    
    free(reader->filename);
    free(reader->record_file);
    free(reader->bytes);
    
    for (size_t i = 0; i < reader->notes_count; i++) {
        free(reader->notes[i].note);
    }
    free(reader->notes);
    
    for (size_t i = 0; i < reader->log_count; i++) {
        free(reader->log_entries[i]);
    }
    free(reader->log_entries);
    
    free(reader);
}

int check_start_sequence(MidiReader* reader) {
    for (int i = 0; i < 3; i++) {
        if (reader->start_counter[i] == (i == 2 ? 1 : 4)) {
            return 1;
        }
    }
    return 0;
}

void skip_bytes(MidiReader* reader, size_t count) {
    reader->itr += count;
    if (reader->itr > reader->bytes_size) {
        reader->itr = reader->bytes_size;
    }
}

uint32_t read_variable_length(MidiReader* reader) {
    if (!reader->bytes || reader->itr >= reader->bytes_size) {
        return 0;
    }
    
    uint32_t value = 0;
    uint8_t byte;
    
    do {
        if (reader->itr >= reader->bytes_size) break;
        
        byte = reader->bytes[reader->itr++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);
    
    return value;
}

void read_mthd(MidiReader* reader) {
    reader->header_length = get_int(reader, 4);
    log_message(reader, "HeaderLength: %u", reader->header_length);
    
    reader->format = get_int(reader, 2);
    reader->tracks = get_int(reader, 2);
    
    uint16_t div = get_int(reader, 2);
    reader->division_type = (div & 0x8000) >> 15;
    reader->division = div & 0x7FFF;
    
    log_message(reader, "Format: %d, Tracks: %d, DivisionType: %d, Division: %d", 
                reader->format, reader->tracks, reader->division_type, reader->division);
}

void read_mtrk(MidiReader* reader) {
    uint32_t length = get_int(reader, 4);
    log_message(reader, "MTrk len: %u", length);
    
    read_midi_track_event(reader, length);
}

char* read_text(MidiReader* reader, size_t length) {
    if (reader->itr + length > reader->bytes_size) {
        length = reader->bytes_size - reader->itr;
    }
    
    char* text = (char*)malloc(length + 1);
    if (!text) return NULL;
    
    for (size_t i = 0; i < length; i++) {
        text[i] = reader->bytes[reader->itr++];
    }
    text[length] = '\0';
    
    return text;
}

int read_midi_meta_event(MidiReader* reader, uint32_t deltaT) {
    if (reader->itr >= reader->bytes_size) return 0;
    
    uint8_t type = reader->bytes[reader->itr++];
    uint32_t length = read_variable_length(reader);
    
    const char* eventName = "Unknown Event";
    for (int i = 0; typeDict[i].description != NULL; i++) {
        if (typeDict[i].code == type) {
            eventName = typeDict[i].description;
            break;
        }
    }
    
    log_message(reader, "MIDIMETAEVENT: %s, LENGTH: %u, DT: %u", eventName, length, deltaT);
    
    if (type == 0x2F) {
        log_message(reader, "END TRACK");
        skip_bytes(reader, 2);
        return 0;
    } else if (type >= 0x01 && type <= 0x0C && type != 0x0B) {
        char* text = read_text(reader, length);
        if (text) {
            log_message(reader, "\t%s", text);
            free(text);
        }
    } else if (type == 0x51) {
        uint32_t tempoValue = get_int(reader, 3);
        reader->tempo = 60000000.0 / tempoValue;

        if (reader->notes_count >= reader->notes_capacity) {
            reader->notes_capacity = reader->notes_capacity ? reader->notes_capacity * 2 : 16;
            reader->notes = realloc(reader->notes, sizeof(MidiNote) * reader->notes_capacity);
        }
        
        reader->notes[reader->notes_count].time = reader->delta_time / reader->division;
        reader->notes[reader->notes_count].note = malloc(32);
        snprintf(reader->notes[reader->notes_count].note, 32, "tempo=%.0f", reader->tempo);
        reader->notes_count++;
        
        log_message(reader, "\tNew tempo is %.0f", reader->tempo);
    } else {
        skip_bytes(reader, length);
    }
    
    return 1;
}

void read_midi_track_event(MidiReader* reader, uint32_t length) {
    if (!reader->bytes) {
        log_message(reader, "No MIDI data to read. Skipping track event.");
        return;
    }
    
    log_message(reader, "TRACKEVENT");
    reader->delta_time = 0;
    
    size_t start = reader->itr;
    int continue_flag = 1;
    
    while (reader->itr - start < length && continue_flag) {
        uint32_t deltaT = read_variable_length(reader);
        reader->delta_time += deltaT;
        
        if (reader->itr >= reader->bytes_size) {
            log_message(reader, "Reached end of MIDI data unexpectedly.");
            break;
        }
        
        if (reader->bytes[reader->itr] == 0xFF) {
            reader->itr++;
            continue_flag = read_midi_meta_event(reader, deltaT);
        } else if (reader->bytes[reader->itr] >= 0xF0 && reader->bytes[reader->itr] <= 0xF7) {
            reader->running_status_set = 0;
            reader->running_status = -1;
            log_message(reader, "RUNNING STATUS SET: CLEARED");
        } else {
            read_voice_event(reader, deltaT);
        }
    }
    
    log_message(reader, "End of MTrk event, jumping from %zu to %zu", reader->itr, start + length);
    reader->itr = start + length;
}

void read_voice_event(MidiReader* reader, uint32_t deltaT) {
    if (reader->itr >= reader->bytes_size) return;
    
    uint8_t type;
    uint8_t channel;
    
    if (reader->bytes[reader->itr] < 0x80 && reader->running_status_set) {
        type = reader->running_status;
        channel = type & 0x0F;
    } else {
        type = reader->bytes[reader->itr];
        channel = type & 0x0F;
        
        if (type >= 0x80 && type <= 0xF7) {
            log_message(reader, "RUNNING STATUS SET: 0x%02X", type);
            reader->running_status = type;
            reader->running_status_set = 1;
        }
        reader->itr++;
    }
    
    if ((type >> 4) == 0x9) {
        if (reader->itr + 1 >= reader->bytes_size) return;
        
        uint8_t key = reader->bytes[reader->itr++];
        uint8_t velocity = reader->bytes[reader->itr++];
        
        int map = key - 23 - 12 - 1;
        while (map >= strlen(reader->piano_scale)) map -= 12;
        while (map < 0) map += 12;
        
        if (velocity == 0) {
            char note_str[2] = {reader->piano_scale[map], '\0'};
            log_message(reader, "%.2f ~%s", reader->delta_time / reader->division, note_str);
            
            if (reader->notes_count >= reader->notes_capacity) {
                reader->notes_capacity = reader->notes_capacity ? reader->notes_capacity * 2 : 16;
                reader->notes = realloc(reader->notes, sizeof(MidiNote) * reader->notes_capacity);
            }
            
            reader->notes[reader->notes_count].time = reader->delta_time / reader->division;
            reader->notes[reader->notes_count].note = malloc(3);
            snprintf(reader->notes[reader->notes_count].note, 3, "~%c", reader->piano_scale[map]);
            reader->notes_count++;
        } else { 
            char note_str[2] = {reader->piano_scale[map], '\0'};
            log_message(reader, "%.2f %s", reader->delta_time / reader->division, note_str);
            
            if (reader->notes_count >= reader->notes_capacity) {
                reader->notes_capacity = reader->notes_capacity ? reader->notes_capacity * 2 : 16;
                reader->notes = realloc(reader->notes, sizeof(MidiNote) * reader->notes_capacity);
            }
            
            reader->notes[reader->notes_count].time = reader->delta_time / reader->division;
            reader->notes[reader->notes_count].note = malloc(2);
            snprintf(reader->notes[reader->notes_count].note, 2, "%c", reader->piano_scale[map]);
            reader->notes_count++;
            
            reader->key_press_count++;
        }
    } else if ((type >> 4) == 0x8) { 
        if (reader->itr + 1 >= reader->bytes_size) return;
        
        uint8_t key = reader->bytes[reader->itr++];
        uint8_t velocity = reader->bytes[reader->itr++];
        
        int map = key - 23 - 12 - 1;
        while (map >= strlen(reader->piano_scale)) map -= 12;
        while (map < 0) map += 12;
        
        char note_str[2] = {reader->piano_scale[map], '\0'};
        log_message(reader, "%.2f ~%s", reader->delta_time / reader->division, note_str);
        
        if (reader->notes_count >= reader->notes_capacity) {
            reader->notes_capacity = reader->notes_capacity ? reader->notes_capacity * 2 : 16;
            reader->notes = realloc(reader->notes, sizeof(MidiNote) * reader->notes_capacity);
        }
        
        reader->notes[reader->notes_count].time = reader->delta_time / reader->division;
        reader->notes[reader->notes_count].note = malloc(3);
        snprintf(reader->notes[reader->notes_count].note, 3, "~%c", reader->piano_scale[map]);
        reader->notes_count++;
    } else if ((type >> 4) != 0x8 && (type >> 4) != 0x9 && 
               (type >> 4) != 0xA && (type >> 4) != 0xB && 
               (type >> 4) != 0xD && (type >> 4) != 0xE) {
        log_message(reader, "VoiceEvent: 0x%02X, 0x%02X, DT: %u", 
                   type, reader->bytes[reader->itr], deltaT);
        reader->itr++;
    } else {
        log_message(reader, "VoiceEvent: 0x%02X, 0x%02X, 0x%02X, DT: %u", 
                   type, reader->bytes[reader->itr], reader->bytes[reader->itr + 1], deltaT);
        reader->itr += 2;
    }
}

void read_events(MidiReader* reader) {
    while (reader->itr + 1 < reader->bytes_size) {
        memset(reader->start_counter, 0, sizeof(reader->start_counter));

        while (reader->itr + 1 < reader->bytes_size && !check_start_sequence(reader)) {
            for (int i = 0; i < 3; i++) {
                if (reader->bytes[reader->itr] == startSequence[i][reader->start_counter[i]]) {
                    reader->start_counter[i]++;
                } else {
                    reader->start_counter[i] = 0;
                }
            }
            
            if (reader->itr + 1 < reader->bytes_size) {
                reader->itr++;
            }
            
            if (reader->start_counter[0] == 4) {
                read_mthd(reader);
            } else if (reader->start_counter[1] == 4) {
                read_mtrk(reader);
            }
        }
    }
}

void log_message(MidiReader* reader, const char* format, ...) {
    if (!reader->verbose && !reader->debug) return;
    
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed < 0) {
        va_end(args);
        return;
    }

    char* buffer = malloc(needed + 1);
    if (!buffer) {
        va_end(args);
        return;
    }
    
    vsnprintf(buffer, needed + 1, format, args);
    va_end(args);
    
    printf("%s\n", buffer);
    
    if (reader->log_count >= reader->log_capacity) {
        reader->log_capacity = reader->log_capacity ? reader->log_capacity * 2 : 16;
        reader->log_entries = realloc(reader->log_entries, sizeof(char*) * reader->log_capacity);
    }
    
    reader->log_entries[reader->log_count++] = buffer;
}

uint32_t get_int(MidiReader* reader, size_t count) {
    if (reader->itr + count > reader->bytes_size) {
        count = reader->bytes_size - reader->itr;
    }
    
    uint32_t value = 0;
    for (size_t i = 0; i < count; i++) {
        value = (value << 8) | reader->bytes[reader->itr++];
    }
    
    return value;
}

void clean_notes(MidiReader* reader) {
    for (size_t i = 0; i < reader->notes_count - 1; i++) {
        for (size_t j = i + 1; j < reader->notes_count; j++) {
            if (reader->notes[i].time > reader->notes[j].time) {
                MidiNote temp = reader->notes[i];
                reader->notes[i] = reader->notes[j];
                reader->notes[j] = temp;
            }
        }
    }
    
    if (reader->verbose) {
        for (size_t i = 0; i < reader->notes_count; i++) {
            printf("%.2f: %s\n", reader->notes[i].time, reader->notes[i].note);
        }
    }
    
    size_t i = 0;
    while (i < reader->notes_count - 1) {
        double a_time = reader->notes[i].time;
        double b_time = reader->notes[i + 1].time;
        
        if (a_time == b_time) {
            char* a_note = reader->notes[i].note;
            char* b_note = reader->notes[i + 1].note;
            
            if (strstr(a_note, "tempo") == NULL && strstr(b_note, "tempo") == NULL &&
                strchr(a_note, '~') == NULL && strchr(b_note, '~') == NULL) {
                size_t new_len = strlen(a_note) + strlen(b_note) + 1;
                char* combined = malloc(new_len);
                if (combined) {
                    snprintf(combined, new_len, "%s%s", a_note, b_note);
                    free(reader->notes[i].note);
                    reader->notes[i].note = combined;

                    free(reader->notes[i + 1].note);
                    for (size_t j = i + 1; j < reader->notes_count - 1; j++) {
                        reader->notes[j] = reader->notes[j + 1];
                    }
                    reader->notes_count--;
                } else {
                    i++;
                }
            } else {
                i++;
            }
        } else {
            i++;
        }
    }

    for (size_t q = 0; q < reader->notes_count; q++) {
        char* note = reader->notes[q].note;
        if (strstr(note, "tempo") == NULL && strchr(note, '~') == NULL) {
            int char_count[256] = {0};
            char* new_note = malloc(strlen(note) + 1);
            if (!new_note) continue;
            
            size_t new_idx = 0;
            for (size_t i = 0; i < strlen(note); i++) {
                unsigned char c = note[i];
                if (char_count[c] == 0) {
                    new_note[new_idx++] = c;
                    char_count[c] = 1;
                }
            }
            new_note[new_idx] = '\0';
            
            free(reader->notes[q].note);
            reader->notes[q].note = new_note;
        }
    }
}

void save_song(MidiReader* reader, const char* song_file) {
    printf("Saving notes to %s\n", song_file);
    
    FILE* file = fopen(song_file, "w");
    if (!file) {
        perror("Error opening song file");
        return;
    }
    
    fprintf(file, "playback_speed=1.1\n");
    for (size_t i = 0; i < reader->notes_count; i++) {
        fprintf(file, "%.2f %s\n", reader->notes[i].time, reader->notes[i].note);
    }
    
    fclose(file);
}

void save_sheet(MidiReader* reader, const char* sheet_file) {
    printf("Saving sheets to %s\n", sheet_file);
    
    FILE* file = fopen(sheet_file, "w");
    if (!file) {
        perror("Error opening sheet file");
        return;
    }
    
    int note_count = 0;
    for (size_t i = 0; i < reader->notes_count; i++) {
        if (strstr(reader->notes[i].note, "tempo") == NULL && 
            strchr(reader->notes[i].note, '~') == NULL) {
            const char* note = reader->notes[i].note;
            
            if (strlen(note) > 1) {
                fprintf(file, "[%s] ", note);
            } else {
                fprintf(file, "%s ", note);
            }
            
            note_count++;
            
            if (note_count % 8 == 0) {
                fprintf(file, "\n");
            }
            
            if (note_count % 32 == 0) {
                fprintf(file, "\n\n");
            }
        }
    }
    
    fclose(file);
}

void save_record(MidiReader* reader, const char* record_file) {
    printf("Saving processing log to %s\n", record_file);
    
    FILE* file = fopen(record_file, "w");
    if (!file) {
        perror("Error opening record file");
        return;
    }
    
    for (size_t i = 0; i < reader->log_count; i++) {
        fprintf(file, "%s\n", reader->log_entries[i]);
    }
    
    fclose(file);
}

void process_midi_file(MidiReader* reader, const char* record_file) {
    FILE* file = fopen(reader->filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open MIDI file %s\n", reader->filename);
        reader->success = 0;
        return;
    }

    fseek(file, 0, SEEK_END);
    reader->bytes_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    reader->bytes = (uint8_t*)malloc(reader->bytes_size);
    if (!reader->bytes) {
        fclose(file);
        fprintf(stderr, "Error: Memory allocation failed\n");
        reader->success = 0;
        return;
    }
    
    size_t bytes_read = fread(reader->bytes, 1, reader->bytes_size, file);
    fclose(file);
    
    if (bytes_read != reader->bytes_size) {
        fprintf(stderr, "Error: Failed to read complete file\n");
        free(reader->bytes);
        reader->bytes = NULL;
        reader->success = 0;
        return;
    }
    
    printf("Processing %s\n", reader->filename);

    read_events(reader);
    
    printf("%u notes processed. Your MIDI survived!\n", reader->key_press_count);
    
    clean_notes(reader);
    reader->success = 1;

    if (reader->success && reader->notes_count > 0) {
        reader->notes[reader->notes_count - 1].time = 1.00;
    }

    save_record(reader, record_file);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <midi_file>\n", argv[0]);
        return 1;
    }
    
    const char* midi_file = argv[1];
    if (!strstr(midi_file, ".mid") && !strstr(midi_file, ".MID")) {
        fprintf(stderr, "Error: File must have .mid extension\n");
        return 1;
    }
    
    MidiReader* reader = midi_reader_init(midi_file);
    if (!reader) {
        fprintf(stderr, "Error: Failed to initialize MIDI reader\n");
        return 1;
    }

    reader->verbose = 1;
    
    process_midi_file(reader, "midiRecord.txt");
    
    if (reader->success) {
        save_song(reader, "song.txt");
        save_sheet(reader, "sheetConversion.txt");
    }
    
    midi_reader_cleanup(reader);
    
    return 0;
}