#ifndef PICO2_JCVM_READER_LED_HPP
#define PICO2_JCVM_READER_LED_HPP

extern "C" void choupi_reader_led_init(void);
extern "C" void choupi_reader_led_set_reader_mode(bool enabled);
extern "C" void choupi_reader_led_activity(void);
extern "C" void choupi_reader_led_task(void);

#endif // PICO2_JCVM_READER_LED_HPP
