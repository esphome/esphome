#ifdef USE_ARDUINO

#include "midi.h"

namespace esphome {
namespace midi_in {

const LogString *midi_type_to_string(midi::MidiType type) {
  switch (type) {
    case midi::MidiType::InvalidType:
      return LOG_STR("InvalidType");
    case midi::MidiType::NoteOff:
      return LOG_STR("NoteOff");
    case midi::MidiType::NoteOn:
      return LOG_STR("NoteOn");
    case midi::MidiType::AfterTouchPoly:
      return LOG_STR("AfterTouchPoly");
    case midi::MidiType::ControlChange:
      return LOG_STR("ControlChange");
    case midi::MidiType::ProgramChange:
      return LOG_STR("ProgramChange");
    case midi::MidiType::AfterTouchChannel:
      return LOG_STR("AfterTouchChannel");
    case midi::MidiType::PitchBend:
      return LOG_STR("PitchBend");
    case midi::MidiType::SystemExclusiveStart:
      return LOG_STR("SystemExclusiveStart");
    case midi::MidiType::TimeCodeQuarterFrame:
      return LOG_STR("TimeCodeQuarterFrame");
    case midi::MidiType::SongPosition:
      return LOG_STR("SongPosition");
    case midi::MidiType::SongSelect:
      return LOG_STR("SongSelect");
    case midi::MidiType::TuneRequest:
      return LOG_STR("TuneRequest");
    case midi::MidiType::SystemExclusiveEnd:
      return LOG_STR("SystemExclusiveEnd");
    case midi::MidiType::Clock:
      return LOG_STR("Clock");
    case midi::MidiType::Tick:
      return LOG_STR("Tick");
    case midi::MidiType::Start:
      return LOG_STR("Start");
    case midi::MidiType::Continue:
      return LOG_STR("Continue");
    case midi::MidiType::Stop:
      return LOG_STR("Stop");
    case midi::MidiType::ActiveSensing:
      return LOG_STR("ActiveSensing");
    case midi::MidiType::SystemReset:
      return LOG_STR("SystemReset");
    default:
      return LOG_STR("Undefined");
  }
}

const LogString *midi_controller_to_string(midi::MidiControlChangeNumber controller) {
  switch (controller) {
    case midi::MidiControlChangeNumber::BankSelect:
      return LOG_STR("BankSelect");
    case midi::MidiControlChangeNumber::ModulationWheel:
      return LOG_STR("ModulationWheel");
    case midi::MidiControlChangeNumber::BreathController:
      return LOG_STR("BreathController");
    case midi::MidiControlChangeNumber::FootController:
      return LOG_STR("FootController");
    case midi::MidiControlChangeNumber::PortamentoTime:
      return LOG_STR("PortamentoTime");
    case midi::MidiControlChangeNumber::DataEntryMSB:
      return LOG_STR("DataEntryMSB");
    case midi::MidiControlChangeNumber::ChannelVolume:
      return LOG_STR("ChannelVolume");
    case midi::MidiControlChangeNumber::Balance:
      return LOG_STR("Balance");
    case midi::MidiControlChangeNumber::Pan:
      return LOG_STR("Pan");
    case midi::MidiControlChangeNumber::ExpressionController:
      return LOG_STR("ExpressionController");
    case midi::MidiControlChangeNumber::EffectControl1:
      return LOG_STR("EffectControl1");
    case midi::MidiControlChangeNumber::EffectControl2:
      return LOG_STR("EffectControl2");
    case midi::MidiControlChangeNumber::GeneralPurposeController1:
      return LOG_STR("GeneralPurposeController1");
    case midi::MidiControlChangeNumber::GeneralPurposeController2:
      return LOG_STR("GeneralPurposeController2");
    case midi::MidiControlChangeNumber::GeneralPurposeController3:
      return LOG_STR("GeneralPurposeController3");
    case midi::MidiControlChangeNumber::GeneralPurposeController4:
      return LOG_STR("GeneralPurposeController4");
    case midi::MidiControlChangeNumber::DataEntryLSB:
      return LOG_STR("DataEntryLSB");
    case midi::MidiControlChangeNumber::Sustain:
      return LOG_STR("Sustain");
    case midi::MidiControlChangeNumber::Portamento:
      return LOG_STR("Portamento");
    case midi::MidiControlChangeNumber::Sostenuto:
      return LOG_STR("Sostenuto");
    case midi::MidiControlChangeNumber::SoftPedal:
      return LOG_STR("SoftPedal");
    case midi::MidiControlChangeNumber::Legato:
      return LOG_STR("Legato");
    case midi::MidiControlChangeNumber::Hold:
      return LOG_STR("Hold");
    case midi::MidiControlChangeNumber::SoundController1:
      return LOG_STR("SoundController1");
    case midi::MidiControlChangeNumber::SoundController2:
      return LOG_STR("SoundController2");
    case midi::MidiControlChangeNumber::SoundController3:
      return LOG_STR("SoundController3");
    case midi::MidiControlChangeNumber::SoundController4:
      return LOG_STR("SoundController4");
    case midi::MidiControlChangeNumber::SoundController5:
      return LOG_STR("SoundController5");
    case midi::MidiControlChangeNumber::SoundController6:
      return LOG_STR("SoundController6");
    case midi::MidiControlChangeNumber::SoundController7:
      return LOG_STR("SoundController7");
    case midi::MidiControlChangeNumber::SoundController8:
      return LOG_STR("SoundController8");
    case midi::MidiControlChangeNumber::SoundController9:
      return LOG_STR("SoundController9");
    case midi::MidiControlChangeNumber::SoundController10:
      return LOG_STR("SoundController10");
    case midi::MidiControlChangeNumber::GeneralPurposeController5:
      return LOG_STR("GeneralPurposeController5");
    case midi::MidiControlChangeNumber::GeneralPurposeController6:
      return LOG_STR("GeneralPurposeController6");
    case midi::MidiControlChangeNumber::GeneralPurposeController7:
      return LOG_STR("GeneralPurposeController7");
    case midi::MidiControlChangeNumber::GeneralPurposeController8:
      return LOG_STR("GeneralPurposeController8");
    case midi::MidiControlChangeNumber::PortamentoControl:
      return LOG_STR("PortamentoControl");
    case midi::MidiControlChangeNumber::Effects1:
      return LOG_STR("Effects1");
    case midi::MidiControlChangeNumber::Effects2:
      return LOG_STR("Effects2");
    case midi::MidiControlChangeNumber::Effects3:
      return LOG_STR("Effects3");
    case midi::MidiControlChangeNumber::Effects4:
      return LOG_STR("Effects4");
    case midi::MidiControlChangeNumber::Effects5:
      return LOG_STR("Effects5");
    case midi::MidiControlChangeNumber::DataIncrement:
      return LOG_STR("DataIncrement");
    case midi::MidiControlChangeNumber::DataDecrement:
      return LOG_STR("DataDecrement");
    case midi::MidiControlChangeNumber::NRPNLSB:
      return LOG_STR("NRPNLSB");
    case midi::MidiControlChangeNumber::NRPNMSB:
      return LOG_STR("NRPNMSB");
    case midi::MidiControlChangeNumber::RPNLSB:
      return LOG_STR("RPNLSB");
    case midi::MidiControlChangeNumber::RPNMSB:
      return LOG_STR("RPNMSB");
    case midi::MidiControlChangeNumber::AllSoundOff:
      return LOG_STR("AllSoundOff");
    case midi::MidiControlChangeNumber::ResetAllControllers:
      return LOG_STR("ResetAllControllers");
    case midi::MidiControlChangeNumber::LocalControl:
      return LOG_STR("LocalControl");
    case midi::MidiControlChangeNumber::AllNotesOff:
      return LOG_STR("AllNotesOff");
    case midi::MidiControlChangeNumber::OmniModeOff:
      return LOG_STR("OmniModeOff");
    case midi::MidiControlChangeNumber::OmniModeOn:
      return LOG_STR("OmniModeOn");
    case midi::MidiControlChangeNumber::MonoModeOn:
      return LOG_STR("MonoModeOn");
    case midi::MidiControlChangeNumber::PolyModeOn:
      return LOG_STR("PolyModeOn");
    default:
      return LOG_STR("Undefined");
  }
}

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
