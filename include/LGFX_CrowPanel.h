#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX(void) {
    // SPI Bus configuration
    {
      auto config = _bus.config();
      config.spi_host = SPI2_HOST;
      config.spi_mode = 0;
      config.freq_write = 80000000;
      config.freq_read = 20000000;
      config.spi_3wire = true;
      config.use_lock = true;
      config.dma_channel = SPI_DMA_CH_AUTO;
      config.pin_sclk = 10;
      config.pin_mosi = 11;
      config.pin_miso = -1;
      config.pin_dc = 3;
      _bus.config(config);
      _panel.setBus(&_bus);
    }

    // Panel configuration
    {
      auto config = _panel.config();
      config.pin_cs = 9;
      config.pin_rst = 14;
      config.pin_busy = -1;
      config.panel_width = 240;
      config.panel_height = 240;
      config.memory_width = 240;
      config.memory_height = 240;
      config.offset_rotation = 0;
      config.offset_x = 0;
      config.offset_y = 0;
      config.dummy_read_pixel = 8;
      config.dummy_read_bits = 1;
      config.readable = false;
      config.invert = true;
      config.rgb_order = false;
      config.dlen_16bit = false;
      config.bus_shared = false;
      _panel.config(config);
    }

    setPanel(&_panel);
  }
};

extern LGFX display;