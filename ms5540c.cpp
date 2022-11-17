#include "ms5540c.h"

#include <Arduino.h>

#include <SPI.h>

ms5540c::ms5540c(int mclk)
: mclk_(mclk) {}

// Функция первичной инициализации датчика, ее необходимо обязательно вызывать в процедуре setup()
// в основном скетче
void ms5540c::init() {
    // Единоразовая установка настроек передачи данных SPI.
    SPI.setBitOrder(MSBFIRST); // Установка порядка передачи битов данных (от старшего бита к младшему)
    // SPI_CLOCK_DIV<N>
    // Установка делителя тактов (заставляет шину SPI работать с частотой,
    // в N раз меньшей, чем исходная тактовая частота микроконтроллера).
    SPI.setClockDivider(SPI_CLOCK_DIV32); 
    pinMode(mclk_, OUTPUT); // Установка разъема mclk_ в режим вывода.
    TCCR1B = (TCCR1B & 0xF8) | 1;
    analogWrite(mclk_, 128);
    this->reset();

    // Чтение PROM для получения коэффициентов заводской калибровки
    int16_t words[4];
    for (int i = 0; i < 4; i++) {
        this->reset();
        words[i] = readWord(i);
    }
    // Извлечение непосредственно коэффициентов из 4х 16-битных слов (согласно datasheet)
    coefs[0] = (words[0] >> 1) & 0x7FFF;
    coefs[1] = ((words[2] & 0x003F) << 6) | (words[3] & 0x003F);
    coefs[2] = (words[3] >> 6) & 0x03FF;
    coefs[3] = (words[2] >> 6) & 0x03FF;
    coefs[4] = ((words[0] & 0x0001) << 10) | ((words[1] >> 6) & 0x03FF);
    coefs[5] = words[1] & 0x003F;
}

// Процедура чтения 16-битного слова, содержащего коэффициенты заводской калибровки,
// из PROM датчика (номер слова widx + 1)
int16_t ms5540c::readWord(int widx) {
    byte recv[2];
    // Отправление запроса на чтение слова под номером widx + 1
    SPI.transfer(WORDS_ACQ[widx*2]);
    SPI.transfer(WORDS_ACQ[widx*2 + 1]);
    // Установка режима передачи данных SPI
    SPI.setDataMode(SPI_MODE1);
    // Т.к. библиотека SPI создана для одновременного чтения/записи, поэтому, чтобы
    // начать чтение данных с шины MISO, необходимо отправить бит-заглушку 0x0 на шину MOSI.
    recv[0] = SPI.transfer(0x0);
    recv[1] = SPI.transfer(0x0);

    return (recv[0] << 8) | recv[1];
}

// Чтение необработанных данных с датчика (процедура предполагает выбор данных - температуры или давления)
int16_t ms5540c::readData(MsrType t) {
    // Чтобы не получить нарушение синхронизации датчика и микроконтроллера,
    // желательно перед проведением каждого чтения данных производить сброс датчика.
    this->reset();
    byte recv[0];
    switch(t) {
        case Temperature:
            SPI.transfer(TMP_MSR[0]);
            SPI.transfer(TMP_MSR[1]);
        break;
        case Pressure:
            SPI.transfer(PRS_MSR[0]);
            SPI.transfer(PRS_MSR[1]);
        break;
    }
    delay(CONV_DUR); // Ожидание завершения конверсии исходных данных на датчике
    SPI.setDataMode(SPI_MODE1);
    recv[0] = SPI.transfer(0x0);
    recv[1] = SPI.transfer(0x0);

    return (recv[0] << 8) | recv[1];
}

// Процедура отправки уникальной последовательности сброса на датчик
// (распознается датчиком в любом состоянии и прерывает любую процедуру,
// выполняемую на датчике в данный момент)
void ms5540c::reset() {
    SPI.setDataMode(SPI_MODE0);
    SPI.transfer(RST_SEQ[0]);
    SPI.transfer(RST_SEQ[1]);
    SPI.transfer(RST_SEQ[2]);
}

// Функция расчета реальной температуры
float ms5540c::getTemperature() {
    const long TEMP = getTempi();
    float TEMPREAL = TEMP / 10.0f;

    return TEMPREAL;
}

// Функция расчета реального давления
float ms5540c::getPressure(UnitType t) {
    const long PCOMP = getPressurei();
    if (t == mmHg) {
        return PCOMP * 750.06 / 10000;
        // mbar*10 -> mmHg === ((mbar/10)/1000)*750.06
    }

    const long TEMP = getTempi();
    if (TEMP < 200 || TEMP > 450) {
        long T2 = 0;
        float P2 = 0;
        if (TEMP < 200) {
            T2 = (11 * (coefs[5] + 24) * (200 - TEMP) * (200 - TEMP) ) >> 20;
            P2 = (3 * T2 * (PCOMP - 3500) ) >> 14;
        }
        else if (TEMP > 45.0) {
            T2 = (3 * (coefs[5] + 24) * (450 - TEMP) * (450 - TEMP) ) >> 20;
            P2 = (T2 * (PCOMP - 10000) ) >> 13;
        }
        const float TEMP2 = TEMP - T2;
        const float PCOMP2 = PCOMP - P2;

        return PCOMP2;
    }

    return PCOMP;
}

// Получение и расчет необработанных данных о температуре
long ms5540c::getTempi() {
    const int16_t D2 = readData(Temperature);
    const long UT1 = (coefs[4] << 3) + 20224;
    const long dT = D2 - UT1;
    const long TEMP = 200 + ((dT * (coefs[5] + 50)) >> 10);

    return TEMP;
}

// Получение и расчет необработанных данных о давлении
long ms5540c::getPressurei() {
    const int16_t D1 = readData(Pressure);
    const int16_t D2 = readData(Temperature);

    const long UT1 = (coefs[4] << 3) + 20224;
    const long dT = D2 - UT1;

    const long OFF  = (coefs[1] * 4) + (((coefs[3] - 512) * dT) >> 12);
    const long SENS = coefs[0] + ((coefs[2] * dT) >> 10) + 24576;
    const long X = (SENS * (D1 - 7168) >> 14) - OFF;
    long PCOMP = ((X * 10) >> 5) + 2500;

    return PCOMP;
}
