/*
 * IncFile1.h
 *
 * Created: 2/12/2016 9:55:45 PM
 *  Author: Michael Haines
 */


#ifndef SYNTH_H
#define SYNTH_H

#define SYNTH_VOICE_COUNT 6
#define FS 32000

#include <Arduino.h>


extern volatile uint16_t amplitude[SYNTH_VOICE_COUNT];           //-Wave amplitudes [0-255]
extern volatile uint16_t cv_pitch[SYNTH_VOICE_COUNT];

extern volatile unsigned char divider;//-Sample rate decimator for envelope
extern volatile uint16_t time_hz;
extern volatile uint32_t tik;
extern volatile unsigned char output_mode;

extern volatile int16_t Pitch_bend[SYNTH_VOICE_COUNT];
extern volatile int noteTrigger[SYNTH_VOICE_COUNT];
extern volatile int noteDeath[SYNTH_VOICE_COUNT];
extern volatile int current_stage;
extern volatile int32_t test_variable;

extern int free_notes[SYNTH_VOICE_COUNT];
extern byte notes[SYNTH_VOICE_COUNT];

extern uint8_t aftertouch;
extern int8_t global_detune;

extern uint8_t cv7;
extern uint8_t cv70;
extern uint8_t cv2;
extern uint8_t cv12;
extern volatile uint16_t filter_total;

//*********************************************************************************************
//  Audio driver interrupt
//*********************************************************************************************

void begin();
void interfaceCheck();
void note_trigger(byte channel, byte pitch, byte velocity);
void NoteRelease(byte channel, byte pitch, byte velocity);
void ControlChange(byte number, byte value);
void potChange(byte number, uint8_t parameter_type);



#endif /* INCFILE1_H_ */
