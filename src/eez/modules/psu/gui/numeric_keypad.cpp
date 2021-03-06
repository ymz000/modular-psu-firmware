/* / mcu / sound.h
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if OPTION_DISPLAY
#include <assert.h>
#include <math.h>
#include <string.h>

#include <eez/sound.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/channel_dispatcher.h>

#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/psu.h>

namespace eez {
namespace psu {
namespace gui {

static NumericKeypad g_numericKeypadsPool[1];

NumericKeypad* getFreeNumericKeypad() {
    for (unsigned int i = 0; i < sizeof (g_numericKeypadsPool) / sizeof(NumericKeypad); ++i) {
        if (g_numericKeypadsPool[i].m_isFree) {
            return &g_numericKeypadsPool[i];
        }
    }
    assert(false);
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

NumericKeypadOptions::NumericKeypadOptions() {
	pageId = PAGE_ID_NUMERIC_KEYPAD;

    this->channelIndex = -1;

    min = NAN;
    max = NAN;

    flags.checkWhileTyping = false;
    flags.option1ButtonEnabled = false;
    flags.option2ButtonEnabled = false;
    flags.signButtonEnabled = false;
    flags.dotButtonEnabled = false;

    editValueUnit = UNIT_UNKNOWN;
}

void NumericKeypadOptions::enableMaxButton() {
    flags.option1ButtonEnabled = true;
    option1ButtonText = "max";
    option1 = maxOption;
}

void NumericKeypadOptions::enableMinButton() {
    flags.option2ButtonEnabled = true;
    option2ButtonText = "min";
    option2 = minOption;
}
void NumericKeypadOptions::enableDefButton() {
    flags.option2ButtonEnabled = true;
    option2ButtonText = "def";
    option2 = defOption;
}

void NumericKeypadOptions::maxOption() {
    getActiveKeypad()->setMaxValue();
}

void NumericKeypadOptions::minOption() {
    getActiveKeypad()->setMinValue();
}

void NumericKeypadOptions::defOption() {
    getActiveKeypad()->setDefValue();
}

////////////////////////////////////////////////////////////////////////////////

void NumericKeypad::init(const char *label, const data::Value &value, NumericKeypadOptions &options,
                         void (*okFloat)(float), void (*okUint32)(uint32_t), void (*cancel)()) {
    Keypad::init(label);

    m_okFloatCallback = okFloat;
    m_okUint32Callback = okUint32;
    m_cancelCallback = cancel;

    m_startValue = value;

    m_options = options;

    if (value.getType() == VALUE_TYPE_IP_ADDRESS) {
        m_options.flags.dotButtonEnabled = true;
    }

    if (m_startValue.isMicro()) {
        switchToMicro();
    } else if (m_startValue.isMilli()) {
        switchToMilli();
    }

    m_minChars = 0;
    m_maxChars = 16;

    reset();

    if (value.getType() == VALUE_TYPE_IP_ADDRESS) {
        ipAddressToString(value.getUInt32(), m_keypadText);
        m_state = BEFORE_DOT;
    }
}

NumericKeypad *NumericKeypad::start(const char *label, const data::Value &value,
                                    NumericKeypadOptions &options, void (*okFloat)(float),
                                    void (*okUint32)(uint32_t), void (*cancel)()) {
    NumericKeypad *page = getFreeNumericKeypad();

    page->init(label, value, options, okFloat, okUint32, cancel);

    pushPage(options.pageId, page);

    return page;
}

bool NumericKeypad::isEditing() {
    return m_state != EMPTY && m_state != START;
}

char NumericKeypad::getDotSign() {
    if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
        return ':';
    }
    return '.';
}

void NumericKeypad::appendEditUnit(char *text) {
    strcat(text, getUnitName(m_options.editValueUnit));
}

void NumericKeypad::getKeypadText(char *text) {
    if (*m_label) {
        strcpy(text, m_label);
        text += strlen(m_label);
    }

    getText(text, 16);
}

bool NumericKeypad::getText(char *text, int count) {
    if (m_state == START) {
        if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            edit_mode::getCurrentValue().toText(text, count);
        } else {
            m_startValue.toText(text, count);
        }
        return false;
    }

    strcpy(text, m_keypadText);

    appendCursor(text);

    appendEditUnit(text);

    return true;
}

////////////////////////////////////////////////////////////////////////////////

Unit NumericKeypad::getEditUnit() {
    return m_options.editValueUnit;
}

Unit NumericKeypad::getValueUnit() {
    if (m_options.editValueUnit == UNIT_MILLI_VOLT)
        return UNIT_VOLT;
    if (m_options.editValueUnit == UNIT_MILLI_AMPER || m_options.editValueUnit == UNIT_MICRO_AMPER)
        return UNIT_AMPER;
    if (m_options.editValueUnit == UNIT_MILLI_SECOND)
        return UNIT_SECOND;
    if (m_options.editValueUnit == UNIT_MILLI_WATT)
        return UNIT_WATT;
    return m_options.editValueUnit;
}

bool NumericKeypad::isMilli() {
    return m_options.editValueUnit == UNIT_MILLI_VOLT ||
           m_options.editValueUnit == UNIT_MILLI_AMPER ||
           m_options.editValueUnit == UNIT_MILLI_WATT ||
           m_options.editValueUnit == UNIT_MILLI_SECOND;
}

bool NumericKeypad::isMicro() {
    return m_options.editValueUnit == UNIT_MICRO_AMPER;
}

Unit NumericKeypad::getMilliUnit() {
    if (m_options.editValueUnit == UNIT_VOLT)
        return UNIT_MILLI_VOLT;
    if (m_options.editValueUnit == UNIT_AMPER || m_options.editValueUnit == UNIT_MICRO_AMPER)
        return UNIT_MILLI_AMPER;
    if (m_options.editValueUnit == UNIT_WATT)
        return UNIT_MILLI_WATT;
    if (m_options.editValueUnit == UNIT_SECOND)
        return UNIT_MILLI_SECOND;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getMicroUnit() {
    if (m_options.editValueUnit == UNIT_AMPER || m_options.editValueUnit == UNIT_MILLI_AMPER)
        return UNIT_MICRO_AMPER;
    return m_options.editValueUnit;
}


void NumericKeypad::switchToMilli() {
    m_options.editValueUnit = getMilliUnit();
}

void NumericKeypad::switchToMicro() {
    m_options.editValueUnit = getMicroUnit();
}

Unit NumericKeypad::getSwitchToUnit() {
    if (m_options.editValueUnit == UNIT_VOLT)
        return UNIT_MILLI_VOLT;
    if (m_options.editValueUnit == UNIT_MILLI_VOLT)
        return UNIT_VOLT;
    if (m_options.editValueUnit == UNIT_AMPER)
        return UNIT_MILLI_AMPER;
    if (m_options.editValueUnit == UNIT_MILLI_AMPER) {
        if (m_options.channelIndex != -1 && Channel::get(m_options.channelIndex).isMicroAmperAllowed()) {
            return UNIT_MICRO_AMPER;
        } else {
            return UNIT_AMPER;
        }
    }
    if (m_options.editValueUnit == UNIT_MICRO_AMPER) {
        if (m_options.channelIndex == -1 || Channel::get(m_options.channelIndex).isAmperAllowed()) {
            return UNIT_AMPER;
        } else {
            return UNIT_MILLI_AMPER;
        }
    }
    if (m_options.editValueUnit == UNIT_WATT)
        return UNIT_MILLI_WATT;
    if (m_options.editValueUnit == UNIT_MILLI_WATT)
        return UNIT_WATT;
    if (m_options.editValueUnit == UNIT_SECOND)
        return UNIT_MILLI_SECOND;
    if (m_options.editValueUnit == UNIT_MILLI_SECOND)
        return UNIT_SECOND;
    return m_options.editValueUnit;
}

void NumericKeypad::toggleEditUnit() {
    m_options.editValueUnit = getSwitchToUnit();
}

////////////////////////////////////////////////////////////////////////////////

float NumericKeypad::getValue() {
    const char *p = m_keypadText;

    int a = 0;
    float b = 0;
    int sign = 1;

    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }

    while (*p && *p != getDotSign()) {
        a = a * 10 + (*p - '0');
        ++p;
    }

    if (*p) {
        const char *q = p + strlen(p) - 1;
        while (q != p) {
            b = (b + (*q - '0')) / 10;
            --q;
        }
    }

    float value = sign * (a + b);

    if (isMicro()) {
        value /= 1000000.0f;
    } else if (isMilli()) {
        value /= 1000.0f;
    }

    return value;
}

int NumericKeypad::getNumDecimalDigits() {
    int n = 0;
    bool afterDot = false;
    for (int i = 0; m_keypadText[i]; ++i) {
        if (afterDot) {
            ++n;
        } else if (m_keypadText[i] == '.') {
            afterDot = true;
        }
    }
    return n;
}

bool NumericKeypad::isValueValid() {
    if (getActivePageId() != PAGE_ID_EDIT_MODE_KEYPAD) {
        return true;
    }

    float value = getValue();

    if (value < m_options.min || value > m_options.max) {
        return false;
    }

    return true;
}

bool NumericKeypad::checkNumSignificantDecimalDigits() {
    return true;
}

void NumericKeypad::digit(int d) {
    if (m_state == START || m_state == EMPTY) {
        m_state = BEFORE_DOT;
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (strlen(m_keypadText) == 0) {
                appendChar('+');
            }
        }
    }
    appendChar(d + '0');

    if (!checkNumSignificantDecimalDigits()) {
        back();
        sound::playBeep();
    }
}

void NumericKeypad::dot() {
    if (!m_options.flags.dotButtonEnabled) {
        return;
    }

    if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
        if (m_state == EMPTY || m_state == START) {
            sound::playBeep();
        } else {
            appendChar(getDotSign());
        }
        return;
    }

    if (m_state == EMPTY) {
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (strlen(m_keypadText) == 0) {
                appendChar('+');
            }
        }
        appendChar('0');
        m_state = BEFORE_DOT;
    }

    if (m_state == START || m_state == EMPTY) {
        appendChar('0');
        m_state = BEFORE_DOT;
    }

    if (m_state == BEFORE_DOT) {
        appendChar(getDotSign());
        m_state = AFTER_DOT;
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::reset() {
    m_state = m_startValue.getType() != VALUE_TYPE_NONE ? START : EMPTY;
    m_keypadText[0] = 0;
}

void NumericKeypad::key(char ch) {
    if (ch >= '0' && ch <= '9') {
        digit(ch - '0');
    } else if (ch == '.') {
        dot();
    }
}

void NumericKeypad::space() {
    // DO NOTHING
}

void NumericKeypad::caps() {
    // DO NOTHING
}

void NumericKeypad::back() {
    int n = strlen(m_keypadText);
    if (n > 0) {
        if (m_keypadText[n - 1] == getDotSign()) {
            m_state = BEFORE_DOT;
        }
        m_keypadText[n - 1] = 0;
        if (n - 1 == 1) {
            if (m_keypadText[0] == '+' || m_keypadText[0] == '-') {
                m_state = EMPTY;
            }
        } else if (n - 1 == 0) {
            m_state = EMPTY;
        }
    } else if (m_state == START) {
        m_state = EMPTY;
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::clear() {
    if (m_state != START) {
        reset();
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::sign() {
    if (m_options.flags.signButtonEnabled) {
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (m_keypadText[0] == 0) {
                m_keypadText[0] = '-';
                m_keypadText[1] = 0;
            } else if (m_keypadText[0] == '-') {
                m_keypadText[0] = '+';
            } else {
                m_keypadText[0] = '-';
            }
        } else {
            if (m_keypadText[0] == '-') {
                strcpy(m_keypadText, m_keypadText + 1);
            } else if (m_keypadText[0] == '+') {
                m_keypadText[0] = '-';
            } else {
                memmove(m_keypadText + 1, m_keypadText, strlen(m_keypadText));
                m_keypadText[0] = '-';
            }

            if (m_state == START || m_state == EMPTY) {
                m_state = BEFORE_DOT;
            }
        }
    } else {
        // not supported
        sound::playBeep();
    }
}

void NumericKeypad::unit() {
    if (m_state == START) {
        m_state = EMPTY;
    }
    toggleEditUnit();
}

void NumericKeypad::option1() {
    if (m_options.flags.option1ButtonEnabled && m_options.option1) {
        m_options.option1();
    }
}

void NumericKeypad::option2() {
    if (m_options.flags.option2ButtonEnabled && m_options.option2) {
        m_options.option2();
    }
}

void NumericKeypad::setMaxValue() {
    m_okFloatCallback(m_options.max);
}

void NumericKeypad::setMinValue() {
    m_okFloatCallback(m_options.min);
}

void NumericKeypad::setDefValue() {
    m_okFloatCallback(m_options.def);
}

bool NumericKeypad::isOkEnabled() {
    return true;
}

void NumericKeypad::ok() {
    if (m_state == START) {
        if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
            m_okFloatCallback(1.0f * m_startValue.getUInt32());
        } else if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            m_okFloatCallback(m_startValue.getInt() / 100.0f);
        } else {
            m_okFloatCallback(m_startValue.isFloat() ? m_startValue.getFloat()
                                                     : m_startValue.getInt());
        }

        return;
    }

    if (m_state != EMPTY) {
        if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
            uint32_t ipAddress;
            if (parseIpAddress(m_keypadText, strlen(m_keypadText), ipAddress)) {
                m_okUint32Callback(ipAddress);
                m_state = START;
                m_keypadText[0] = 0;
            } else {
                errorMessage("Invalid IP address format!");
            }

            return;
        } else {
            float value = getValue();

            if (!isNaN(m_options.min) && value < m_options.min) {
                psuErrorMessage(0, MakeLessThenMinMessageValue(m_options.min, m_startValue));
            } else if (!isNaN(m_options.max) && value > m_options.max) {
                psuErrorMessage(0, MakeGreaterThenMaxMessageValue(m_options.max, m_startValue));
            } else {
                m_okFloatCallback(value);
                m_state = START;
                m_keypadText[0] = 0;
                return;
            }

            return;
        }
    }

    sound::playBeep();
}

void NumericKeypad::cancel() {
    void (*cancel)() = m_cancelCallback;

    popPage();

    if (cancel) {
        cancel();
    }
}

#if OPTION_ENCODER

void NumericKeypad::onEncoderClicked() {
    if (m_state == START) {
        if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            return;
        }
    }
    ok();
}

void NumericKeypad::onEncoder(int counter) {
    if (m_state == START) {
        if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            return;
        }

        if (m_startValue.getType() == VALUE_TYPE_FLOAT) {
            float newValue = encoderIncrement(m_startValue, counter, m_options.min, m_options.max, m_options.channelIndex, 0.01f);
            m_startValue = MakeValue(newValue, m_startValue.getUnit());
            return;
        } else if (m_startValue.getType() == VALUE_TYPE_INT) {
            int newValue = m_startValue.getInt() + counter;

            if (newValue < (int)m_options.min) {
                newValue = (int)m_options.min;
            }

            if (newValue > (int)m_options.max) {
                newValue = (int)m_options.max;
            }

            m_startValue = data::Value(newValue);
            return;
        }
    }

    sound::playBeep();
}

#endif

} // namespace gui
} // namespace psu
} // namespace eez

#endif
