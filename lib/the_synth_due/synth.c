/*
 * CFile1.c
 *
 * Created: 2/12/2016 10:27:58 PM
 *  Author: Michael Haines
 */
#include <Arduino.h>
#include "tables_due.h"
#include "synth.h"
#include "Envelope.h"
#include "Osc.h"
#include "lfo.h"
#include "interface.h"
#include "ramp.h"

static struct{

	struct envelope_struct amplitudeEnvs[SYNTH_VOICE_COUNT];
	struct oscillator_struct oscillators[SYNTH_VOICE_COUNT];
	struct envelope_struct filterEnvs[SYNTH_VOICE_COUNT];
	struct envelope_struct resonantEnvs[SYNTH_VOICE_COUNT];
	struct lfo_struct pitchlfo[SYNTH_VOICE_COUNT];
	struct lfo_struct filterlfo[SYNTH_VOICE_COUNT];
	struct ramp_struct pitchramp[SYNTH_VOICE_COUNT];

	} synthesizer;


volatile uint16_t amplitude[SYNTH_VOICE_COUNT];           //-Wave amplitudes [0-255]
volatile uint16_t cv_pitch[SYNTH_VOICE_COUNT];

volatile unsigned char divider = 0; //-Sample rate decimator for envelope
volatile uint32_t tik = 0;

volatile int16_t Pitch_bend[SYNTH_VOICE_COUNT];

volatile int noteTrigger[SYNTH_VOICE_COUNT];
volatile int noteDeath[SYNTH_VOICE_COUNT];
volatile int current_stage = 0;

volatile int32_t test_variable = 0;

//******************************************************************************
//These are temporary variables and bound to change.
//******************************************************************************

uint16_t amp_out;
uint8_t aftertouch = 0;
int8_t global_detune = 0;
uint8_t cv7 = 0;
uint8_t cv70 = 0;
uint8_t cv2 = 0;
uint8_t cv12 = 0;
volatile uint16_t filter_total = 0;
uint32_t time_stamps[] = {0, 0, 0, 0, 0, 0, 0, 0};

 byte notes[SYNTH_VOICE_COUNT];
 int free_notes[] = {0, 1, 2, 3, 4, 5, 6, 7};


//******************************************************************************
//  Audio driver interrupt
//******************************************************************************

void TC5_Handler(){
	TC_GetStatus(TC1, 2);

	int i = 0;
	//-------------------------------
	// Time division
	//-------------------------------
	divider++;
  if(!(divider%=SYNTH_VOICE_COUNT));
		tik++;

	//-------------------------------
	// Volume and Filter envelope generator
	//-------------------------------
	envelope_update(&synthesizer.amplitudeEnvs[divider]);
	envelope_update(&synthesizer.filterEnvs[divider]);
	envelope_update(&synthesizer.resonantEnvs[divider]);
	ramp_update(&synthesizer.pitchramp[divider]);
	osc_setAmplitude(&synthesizer.oscillators[divider], amplitude[divider]);

	lfo_update(&synthesizer.filterlfo[divider]);
	filter_total = (env_getOutput(&synthesizer.filterEnvs[0]) + (cpParameterList[fltrCutoff] >> 4) +
	aftertouch + lfo_getOutput(&synthesizer.filterlfo[0]));
	if (filter_total > 255) filter_total = 255;

	test_variable = ramp_getOutput(&synthesizer.pitchramp[0]);

	amplitude[divider] = env_getOutput(&synthesizer.amplitudeEnvs[divider]);
	REG_PIOC_CODR = (1 << 9);
	REG_PIOC_OWDR = 0xFFFFFE00;
	REG_PIOC_OWER = ((filter_total) << 1);
	REG_PIOC_ODSR	= ((filter_total) << 1);
	REG_PIOC_SODR = (1 << 9);
	REG_PIOC_OWDR = 0xFFFFE00;
	amp_out = ((1 << 9) | ((env_getOutput(&synthesizer.resonantEnvs[0])) << 1));
	REG_PIOC_OWER = amp_out;
	REG_PIOC_ODSR = amp_out;

	lfo_update(&synthesizer.pitchlfo[divider]);

	osc_setPitch(&synthesizer.oscillators[divider], cpParameterList[oscAfreq] +
		 ((lfo_getOutput(&synthesizer.pitchlfo[divider]) * cpParameterList[oscAMod])>>12) +
		 synthesizer.oscillators[divider].cv_pitch[0] + ramp_getOutput(&synthesizer.pitchramp[divider]), 0);

	osc_setPitch(&synthesizer.oscillators[divider], synthesizer.oscillators[divider].cv_pitch[1] +
		((lfo_getOutput(&synthesizer.pitchlfo[divider]) * cpParameterList[oscBMod])>>12) +
		cpParameterList[oscBfreq] + ramp_getOutput(&synthesizer.pitchramp[divider]), 1);

	//-------------------------------
	//  Synthesizer/audio mixer
	//-------------------------------

	int16_t output_sum = 0;
	for(i=0; i<SYNTH_VOICE_COUNT; i++){
			osc_update(&synthesizer.oscillators[i]);
			output_sum += osc_getOutput(&synthesizer.oscillators[i]);
	}

	output_sum = 127 + ((output_sum) >> 4);

	REG_PIOD_CODR = (1 << 9);
	REG_PIOD_OWDR = 0xFFFFFF00;
	REG_PIOD_OWER = output_sum;
	REG_PIOD_ODSR	= output_sum;
	REG_PIOD_SODR = (1 << 9);
	REG_PIOD_OWDR = 0xFFFFFF00;
	REG_PIOD_OWER = (cpParameterList[fltrResonance]>>4);
	REG_PIOD_ODSR = (cpParameterList[fltrResonance]>>4);
	//REG_PIOD_ODSR = 127 + ((output_sum) >> 3);

	osc_updateFrequancyTuningWord(&synthesizer.oscillators[divider]);

}
//------------------------------------------------------------------------------
//Update the interface and check for any human interactions.
//------------------------------------------------------------------------------

void interfaceCheck(){
	interfaceUpdate();
}

//------------------------------------------------------------------------------
// Anything below this line is temporary and subject to change with the
// exception of function begin()
//------------------------------------------------------------------------------

void set_envelopes(){

	int i = 0;
	for(i = 0; i < SYNTH_VOICE_COUNT; i++){
		envelope_setup(&synthesizer.amplitudeEnvs[i], 65535,65535,65535,500);
		envelope_setup(&synthesizer.filterEnvs[i], 65535,65535,65535,1);
		//envelope_setup(&synthesizer.resonantEnvs[i], 46,56,45333,19);
		envelope_setup(&synthesizer.resonantEnvs[i], 65535,75,65535,500);
		ramp_setup(&synthesizer.pitchramp[i], 160);
	}

}


void set_oscillators(){
	int i = 0;
	for(i = 0; i < SYNTH_VOICE_COUNT - 1; i++){
		//setVoices(&synthesizer.oscillators[i], &string_C6, 0, 127);
		osc_setWaves(&synthesizer.oscillators[i], &waveStruct[3],0, 127, 0);
		osc_setWaves(&synthesizer.oscillators[i], &waveStruct[3],0, 127, 1);
	}

	//setVoices(&synthesizer.oscillators[7], &snare,0,127);

}

void set_lfo(){
	int i = 0;
	for (i = 0; i< SYNTH_VOICE_COUNT; i++){
		lfo_init(&synthesizer.pitchlfo[i], lfoSine, 0, 0);
		lfo_init(&synthesizer.filterlfo[i], lfoSine, 0, 0);

	}
}

void begin()
{
			//initialize interface
			interfaceInit();

			//initialize interrupt
			pmc_set_writeprotect(false);
			pmc_enable_periph_clk((uint32_t)TC5_IRQn);
			TC_Configure(TC1, 2, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
			uint32_t rc = VARIANT_MCK/128/FS; //128 because we selected TIMER_CLOCK4 above
			//TC_SetRA(TC1, 2, rc/2); //50% high, 50% low
			TC_SetRC(TC1, 2, rc);
			TC_Start(TC1, 2);
			TC1->TC_CHANNEL[2].TC_IER=TC_IER_CPCS;
			TC1->TC_CHANNEL[2].TC_IDR=~TC_IER_CPCS;
			NVIC_EnableIRQ(TC5_IRQn);

			set_envelopes();
			set_lfo();
}

void mTrigger(unsigned char voice,unsigned char MIDInote, uint16_t given_amplitude,
	int8_t detune)
{

	uint16_t temp_pitch[2] = {MIDInote, MIDInote};
	uint8_t osc_amp[2] = {cpParameterList[oscAVol], cpParameterList[oscBVol]};
	osc_trigger(&synthesizer.oscillators[voice], temp_pitch, MIDInote, osc_amp);
	cv_pitch[voice] = 240 * MIDInote;
	lfo_trigger(&synthesizer.pitchlfo[voice]);
	lfo_trigger(&synthesizer.filterlfo[voice]);
	ramp_trigger(&synthesizer.pitchramp[voice], cpParameterList[rampAmount] * 2);
	envelope_trigger(&synthesizer.amplitudeEnvs[voice], 65535);
	if(current_stage <=1){
		envelope_trigger(&synthesizer.filterEnvs[0], cpParameterList[fltrEnvMnt] * 16);
		envelope_trigger(&synthesizer.resonantEnvs[0], given_amplitude);
	}
	noteTrigger[voice] = 1;
	}

void note_trigger(byte channel, byte given_pitch, byte velocity){

	int8_t voice=-1;
	uint8_t i = 0;
	uint32_t oldest_time_stamp=UINT32_MAX;
	for(i = 0; i < SYNTH_VOICE_COUNT; i++){
		if(free_notes[i] > -1){
			//test_variable = env_getStage(&synthesizer.amplitudeEnvs[i]);
			if (env_getStage(&synthesizer.amplitudeEnvs[i]) == DEAD){
					voice = i;
					break;
			}
			if(time_stamps[i]<oldest_time_stamp){
					oldest_time_stamp=time_stamps[i];
					voice = i;
			}
		}
	}
	if (voice==-1){
		return;
	}
	notes[voice] = given_pitch;
	free_notes[voice] = -1;
	current_stage += 1;
	mTrigger(voice, given_pitch, velocity * 516, global_detune);
}


void NoteRelease(byte channel, byte given_pitch, byte velocity){
	int i = 0;
	for(i = 0; i < SYNTH_VOICE_COUNT; i++){
			if(notes[i] == given_pitch){
				  noteDeath[i] = 1;
					free_notes[i] = i;
					time_stamps[i] = tik;

					envelope_setStage(&synthesizer.amplitudeEnvs[i],RELEASE);
		}

	}

	current_stage -= 1;
	if (current_stage <= 0){
		envelope_setStage(&synthesizer.filterEnvs[0],RELEASE);
		envelope_setStage(&synthesizer.resonantEnvs[0],RELEASE);
		current_stage = 0;
	}
}

void ControlChange(byte number, byte value){
		int i = 0;
		switch(number){
			case 11:
					cpParameterList[fltrCutoff] = ((value) * 2);
					break;
			case 5:
					cv2 = value;
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							lfo_setRate(&synthesizer.pitchlfo[i], cv2 * cv70 * 2);
						}
						break;
			case 13:
					cv70 = value;
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							lfo_setRate(&synthesizer.pitchlfo[i], cv2 * cv70 * 2);
						}
						break;
			case 12:
					cv12 = value;
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							lfo_init(&synthesizer.pitchlfo[i], synthesizer.pitchlfo[i].shape, cv12 * cv7 * 2, synthesizer.pitchlfo[i].rate);
						}
						break;
			case 2: //7
					cv7 = value;
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							lfo_init(&synthesizer.pitchlfo[i], synthesizer.pitchlfo[i].shape, cv12 * cv7 * 2, synthesizer.pitchlfo[i].rate);
						}
						break;
			case 84:
					global_detune = ((value + 1) * .5) - 32;
					break;
			case 70:
					cpParameterList[fltrEnvMnt] = (512 * (value + 1)) - 1;
					break;
			case 15: //10
					cpParameterList[fltrResonance] = (512 * (value + 1)) - 1;
					break;
			case 71:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							if (value == 127){
								envelope_setup(&synthesizer.resonantEnvs[i],
									65535, synthesizer.resonantEnvs[i].decayIncreament,
									synthesizer.resonantEnvs[i].sustainCV, synthesizer.resonantEnvs[i].releaseIncreament);
								}
							else{
							envelope_setup(&synthesizer.resonantEnvs[i],
								(15 * (value)) + 1, synthesizer.resonantEnvs[i].decayIncreament,
								synthesizer.resonantEnvs[i].sustainCV, synthesizer.resonantEnvs[i].releaseIncreament);
							}
						}
						break;
			case 72:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.resonantEnvs[i],
								synthesizer.resonantEnvs[i].attackIncreament, (15 * (value )) + 1,
								synthesizer.resonantEnvs[i].sustainCV, synthesizer.resonantEnvs[i].releaseIncreament);
						}
						break;
			case 73:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.resonantEnvs[i],
								synthesizer.resonantEnvs[i].attackIncreament, synthesizer.resonantEnvs[i].decayIncreament,
								(512 * (value + 1)) - 1, synthesizer.resonantEnvs[i].releaseIncreament);
						}
						break;
			case 74:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.resonantEnvs[i],
								synthesizer.resonantEnvs[i].attackIncreament, synthesizer.resonantEnvs[i].decayIncreament,
								synthesizer.resonantEnvs[i].sustainCV, (12 * (value )) + 1);
						}
						break;
			case 75:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							if (value == 127){
								envelope_setup(&synthesizer.filterEnvs[i],
									65535, synthesizer.filterEnvs[i].decayIncreament,
									synthesizer.filterEnvs[i].sustainCV, synthesizer.filterEnvs[i].releaseIncreament);
							}
							else{
								envelope_setup(&synthesizer.filterEnvs[i],
									(15 * (value)) + 1, synthesizer.filterEnvs[i].decayIncreament,
									synthesizer.filterEnvs[i].sustainCV, synthesizer.filterEnvs[i].releaseIncreament);
							}
						}
						break;
			case 76:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.filterEnvs[i],
								synthesizer.filterEnvs[i].attackIncreament, (15 * (value)) + 1,
								synthesizer.filterEnvs[i].sustainCV, synthesizer.filterEnvs[i].releaseIncreament);
						}
						break;
			case 77:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.filterEnvs[i],
								synthesizer.filterEnvs[i].attackIncreament, synthesizer.filterEnvs[i].decayIncreament,
								(512 * (value + 1)) - 1, synthesizer.filterEnvs[i].releaseIncreament);
						}
						break;
			case 78:
					for (i = 0; i < SYNTH_VOICE_COUNT; i++){
							envelope_setup(&synthesizer.filterEnvs[i],
								synthesizer.filterEnvs[i].attackIncreament, synthesizer.filterEnvs[i].decayIncreament,
								synthesizer.filterEnvs[i].sustainCV, (12 * (value )) + 1);
						}
						break;
		}
}

void potChange(byte number, uint8_t parameter_type){
	uint16_t i;
	uint8_t temp_value;
	if (!parameter_type){
		switch (number) {
				case 4:
					for(i=0; i<SYNTH_VOICE_COUNT; i++){
						if (cpParameterList[oscSync] >> 11) osc_setSync(&synthesizer.oscillators[i], osmMaster);
						else osc_setSync(&synthesizer.oscillators[i], osmNone);
					}
				case 8:
						global_detune = (((cpParameterList[oscDetune] >> 4) + 1) / 2) - 64;
						break;
				case 13:
				case 14:
				case 15:
				case 16:
						if (cpParameterList[number] >= 4080){
								cpParameterList[number] = 65535;
							}
						for (i = 0; i < SYNTH_VOICE_COUNT; i++){
								envelope_setup(&synthesizer.filterEnvs[i],cpParameterList[fltrAtt] >> 2, cpParameterList[fltrDec] >> 2,
									16 * cpParameterList[fltrSus], cpParameterList[fltrRel] >> 2);
						}
						break;
				case 17:
				case 18:
				case 19:
				case 20:
						if (cpParameterList[number] >= 4080){
								cpParameterList[number] = 65535;
							}
						for (i = 0; i < SYNTH_VOICE_COUNT; i++){
								envelope_setup(&synthesizer.amplitudeEnvs[i],cpParameterList[AmpAtt] >> 2, cpParameterList[AmpDec] >> 2,
									16 * cpParameterList[AmpSus], cpParameterList[AmpRel] >> 2);
						}
						break;
				case 21:
				case 22:
				case 23:
				case 24:
						if (cpParameterList[number] >= 4080){
								cpParameterList[number] = 65535;
							}
						for (i = 0; i < SYNTH_VOICE_COUNT; i++){
								envelope_setup(&synthesizer.resonantEnvs[i],cpParameterList[AuxAtt] >> 2, cpParameterList[AuxDec] >> 2,
									16 * cpParameterList[AuxSus], cpParameterList[AuxRel] >> 2);
						}
						break;
				case 25:
				case 26:
				case 27:
				case 28:
						for (i = 0; i < SYNTH_VOICE_COUNT; i++){
								lfo_init(&synthesizer.pitchlfo[i], cpParameterList[lfoAShape]/1000, cpParameterList[lfoAPitch], cpParameterList[lfoARate]);
							}
						break;

				case 29:
				case 30:
				case 31:
				case 32:
						for (i = 0; i < SYNTH_VOICE_COUNT; i++){
								lfo_init(&synthesizer.filterlfo[i], cpParameterList[lfoBShape]/1000, cpParameterList[lfoBPitch] >> 4, cpParameterList[lfoBRate]);
							}
						break;

				case 33:
					for (i = 0; i <SYNTH_VOICE_COUNT; i++){
						ramp_setup(&synthesizer.pitchramp[i], cpParameterList[number]);
					}
					break;
			}
		}
		else{
			switch (number) {
				case 1:
					temp_value = spParameterList[number] >> 8;
					if (temp_value > 11){
						temp_value = 11;
						spParameterList[number] = temp_value << 8;
					}
					for (i = 0; i <SYNTH_VOICE_COUNT; i++){
						osc_setWaves(&synthesizer.oscillators[i], &waveStruct[temp_value], 0, 127, 0);
					}
					break;
				case 2:
					temp_value = spParameterList[number] >> 8;
					if (temp_value > 11){
						temp_value = 11;
						spParameterList[number] = temp_value << 8;
					}
					for (i = 0; i <SYNTH_VOICE_COUNT; i++){
						osc_setWaves(&synthesizer.oscillators[i], &waveStruct[temp_value], 0, 127, 1);
					}
					break;
			}
		}
}
