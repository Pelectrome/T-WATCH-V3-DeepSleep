#include "config.h"
#include <btAudio.h> //for ble audio
#include <webDSP.h>  //for ble audio

#define REPEAT_CAL false

bool SwitchOn = false;
// Comment out to stop drawing black spots
#define BLACK_SPOT

// Switch position and size
#define FRAME_X 87
#define FRAME_Y 7
#define FRAME_W 50
#define FRAME_H 25

// Red zone size
#define REDBUTTON_X FRAME_X
#define REDBUTTON_Y FRAME_Y
#define REDBUTTON_W (FRAME_W / 2)
#define REDBUTTON_H FRAME_H

// Green zone size
#define GREENBUTTON_X (REDBUTTON_X + REDBUTTON_W)
#define GREENBUTTON_Y FRAME_Y
#define GREENBUTTON_W (FRAME_W / 2)
#define GREENBUTTON_H FRAME_H

TTGOClass *ttgo;
AXP20X_Class *power;

btAudio audio = btAudio("Hello"); // for ble audio

void go_to_sleep();

void greenBtn();
void redBtn();
void drawFrame();

char buf[128];
bool irq = false;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

void setup()
{
    Serial.begin(115200);
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    ttgo->openBL();

    power = ttgo->power;
    power->adc1Enable(
        AXP202_VBUS_VOL_ADC1 |
            AXP202_VBUS_CUR_ADC1 |
            AXP202_BATT_CUR_ADC1 |
            AXP202_BATT_VOL_ADC1,
        true);

    ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
    ledcAttachPin(15, pwmLedChannelTFT);
    ledcWrite(pwmLedChannelTFT, 30);

    ttgo->tft->setRotation(0);
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextFont(2);
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->drawString("Benachour Sohaib", 62, 50);

    redBtn();

    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(
        AXP202_INT, []
        { irq = true; },
        FALLING);
    //! Clear IRQ unprocessed  first
    ttgo->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    ttgo->power->clearIRQ();
}

long ttt = 0;
String Time;
String Date;
String BtPer;
String BtDisCur;
int16_t x, y;

void loop()
{

    if (ttgo->getTouch(x, y))
    {
        if(x>200 && x<255)
        ledcWrite(pwmLedChannelTFT, 255-y);

        sprintf(buf, "x:%03d  y:%03d", x, y);
        ttgo->tft->drawString(buf, 80, 80);
        ttt = millis();

        if (SwitchOn)
        {
            if ((x > FRAME_X) && (x < (FRAME_X + FRAME_W)))
            {
                if ((y > FRAME_Y) && (y <= (FRAME_Y + FRAME_H)))
                {
                    Serial.println("Red btn hit");
                    redBtn();

                    audio.disconnect();
                    ttgo->disableAudio();

                    delay(200);
                }
            }
        }
        else // Record is off (SwitchOn == false)
        {
            if ((x > FRAME_X) && (x < (FRAME_X + FRAME_W)))
            {
                if ((y > FRAME_Y) && (y <= (FRAME_Y + FRAME_H)))
                {
                    Serial.println("Green btn hit");
                    greenBtn();

                    ttgo->enableLDO3(); // for ble audio
                    audio.begin();      // for ble audio
                    //  attach to pins  // for ble audio
                    int bck = TWATCH_DAC_IIS_BCK;
                    int ws = TWATCH_DAC_IIS_WS;
                    int dout = TWATCH_DAC_IIS_DOUT;
                    audio.I2S(bck, dout, ws);

                    delay(200);
                }
            }
        }
    }

    if (millis() > ttt + 10000 && SwitchOn == false)
    {
        go_to_sleep();
    }
    BtPer = String(power->getBattPercentage());
    BtDisCur = String(power->getBattDischargeCurrent());
    Time = String(ttgo->rtc->formatDateTime());
    Date = String(ttgo->rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY));

    ttgo->tft->setTextColor(TFT_GREEN, TFT_BLACK);
    ttgo->tft->drawString(BtPer + "%", 204, 15);
    ttgo->tft->setTextColor(TFT_ORANGE, TFT_BLACK);
    ttgo->tft->drawString(BtDisCur + "mA", 20, 15);
    ttgo->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    ttgo->tft->drawString(Time, 10, 130, 7);
    ttgo->tft->drawString(Date, 60, 200, 4);

    if (irq)
    {
        irq = false;
        ttgo->power->readIRQ();
        if (ttgo->power->isPEKShortPressIRQ())
        {
            go_to_sleep();
        }
        ttgo->power->clearIRQ();
    }
}

void go_to_sleep()
{
    // Clean power chip irq status
    ttgo->power->clearIRQ();

    // Set  touchscreen sleep
    ttgo->displaySleep();

    /*
    In TWatch2019/ Twatch2020V1, touch reset is not connected to ESP32,
    so it cannot be used. Set the touch to sleep,
    otherwise it will not be able to wake up.
    Only by turning off the power and powering on the touch again will the touch be working mode
    // ttgo->displayOff();
    */

    ttgo->powerOff();

    // Set all channel power off
    ttgo->power->setPowerOutPut(AXP202_LDO3, false);
    ttgo->power->setPowerOutPut(AXP202_LDO4, false);
    ttgo->power->setPowerOutPut(AXP202_LDO2, false);
    ttgo->power->setPowerOutPut(AXP202_EXTEN, false);
    ttgo->power->setPowerOutPut(AXP202_DCDC2, false);

    // TOUCH SCREEN  Wakeup source
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);
    // PEK KEY  Wakeup source
    // esp_sleep_enable_ext1_wakeup(0x4800000000UL, ESP_EXT1_WAKEUP_ALL_LOW);
    // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
}
void redBtn()
{
    ttgo->tft->fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_RED);
    ttgo->tft->fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_DARKGREY);
    drawFrame();
    // ttgo->tft->setTextColor(TFT_WHITE);
    // ttgo->tft->setTextSize(2);
    ttgo->tft->setTextDatum(MC_DATUM);
    ttgo->tft->drawString("ON", GREENBUTTON_X + (GREENBUTTON_W / 2), GREENBUTTON_Y + (GREENBUTTON_H / 2));
    ttgo->tft->setTextDatum(false);
    SwitchOn = false;
}
// Draw a green button
void greenBtn()
{
    ttgo->tft->fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_GREEN);
    ttgo->tft->fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_DARKGREY);
    drawFrame();
    ttgo->tft->setTextColor(TFT_WHITE);
    // ttgo->tft->setTextSize(2);
    ttgo->tft->setTextDatum(MC_DATUM);
    ttgo->tft->drawString("OFF", REDBUTTON_X + (REDBUTTON_W / 2) + 1, REDBUTTON_Y + (REDBUTTON_H / 2));
    ttgo->tft->setTextDatum(false);
    SwitchOn = true;
}
void drawFrame()
{
    ttgo->tft->drawRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, TFT_BLACK);
}