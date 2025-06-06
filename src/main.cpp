/* Bi-directionnal chain counter based on SensESP */

#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/debounce.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/ui_controls.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include <Preferences.h>

using namespace sensesp;

/* Prepare application */
void setup() {
  SetupLogging();
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("ChainCounter")
                    ->get_app();
  static reactesp::Event* delayptr = nullptr;

  /* Default values */
  float gypsy_circum_default = 0.25; //Default 0.25m
  float up_delay_default     = 2000; //Default 2000ms = 2s
  float down_delay_default   = 2000; //Default 2000ms = 2s
  float di1_gpio_default     = 23;   // Default GPIO 23 = Digital Input 1 in HALMET
  float di1_dtime_default    = 15;   // Default 15 ms
  float di2_gpio_default     = 25;   // Default GPIO 25 = Digital Input 2 in HALMET
  float di2_dtime_default    = 15;   // Default 15 ms
  float di3_gpio_default     = 27;   // Default GPIO 27 = Digital Input 3 in HALMET
  float di3_dtime_default    = 15;   // Default 15 ms
  float di4_gpio_default     = 26;   // Default GPIO 26 = Digital Input 5 in HALMET
  float di4_dtime_default    = 15;   // Default 15 ms

  /* Save path */
  String gypsy_circum_config_path = "/gypsy/circum";
  String up_delay_config_path     = "/up/delay";
  String down_delay_config_path   = "/down/delay";
  String di1_gpio_config_path     = "/di1/gpio";
  String di1_dtime_config_path    = "/di1/dbounce";
  String di2_gpio_config_path     = "/di2/gpio";
  String di2_dtime_config_path    = "/di2/dbounce";
  String di3_gpio_config_path     = "/di3/gpio";
  String di3_dtime_config_path    = "/di3/dbounce";
  String di4_gpio_config_path     = "/di4/gpio";
  String di4_dtime_config_path    = "/di4/dbounce";

  /* Register config parameters */
  auto gypsy_circum_config = std::make_shared<NumberConfig>(gypsy_circum_default,  gypsy_circum_config_path );
  auto up_delay_config     = std::make_shared<NumberConfig>(up_delay_default,      up_delay_config_path     );
  auto down_delay_config   = std::make_shared<NumberConfig>(down_delay_default,    down_delay_config_path   );
  auto di1_gpio_config     = std::make_shared<NumberConfig>(di1_gpio_default,      di1_gpio_config_path     );
  auto di1_dtime_config    = std::make_shared<NumberConfig>(di1_dtime_default,     di1_dtime_config_path    );
  auto di2_gpio_config     = std::make_shared<NumberConfig>(di2_gpio_default,      di2_gpio_config_path     );
  auto di2_dtime_config    = std::make_shared<NumberConfig>(di2_dtime_default,     di2_dtime_config_path    );
  auto di3_gpio_config     = std::make_shared<NumberConfig>(di3_gpio_default,      di3_gpio_config_path     );
  auto di3_dtime_config    = std::make_shared<NumberConfig>(di3_dtime_default,     di3_dtime_config_path    );
  auto di4_gpio_config     = std::make_shared<NumberConfig>(di4_gpio_default,      di4_gpio_config_path     );
  auto di4_dtime_config    = std::make_shared<NumberConfig>(di4_dtime_default,     di4_dtime_config_path    );

  /* Set parameters in UI */
  ConfigItem(gypsy_circum_config)
    ->set_title("Gypsy Circumference")
    ->set_description("Circumference of the gypsy in meters")
    ->set_sort_order(100);
  ConfigItem(up_delay_config)
    ->set_title("Up delay")
    ->set_description("Time after a push to up button go to free fall")
    ->set_sort_order(200);
  ConfigItem(down_delay_config)
    ->set_title("Down delay")
    ->set_description("Time after a push to down button go to free fall")
    ->set_sort_order(200);
  ConfigItem(di1_gpio_config)
    ->set_title("GPIO for UP button")
    ->set_description("GPIO number connected to UP Button relay")
    ->set_sort_order(1100);
  ConfigItem(di1_dtime_config)
    ->set_title("Debounce Time for UP button")
    ->set_description("Debounce time in ms for UP Button relay")
    ->set_sort_order(1150);
  ConfigItem(di2_gpio_config)
    ->set_title("GPIO for DOWN button")
    ->set_description("GPIO number connected to DOWN Button relay")
    ->set_sort_order(1200);
  ConfigItem(di2_dtime_config)
    ->set_title("Debounce Time for UP button")
    ->set_description("Debounce time in ms for DOWN Button relay")
    ->set_sort_order(1250);
  ConfigItem(di3_gpio_config)
    ->set_title("GPIO for Hall effect sensor")
    ->set_description("GPIO number connected to hall effect sensor")
    ->set_sort_order(1300);
  ConfigItem(di3_dtime_config)
    ->set_title("Debounce Time for hall effect sensor")
    ->set_description("Debounce time in ms for hall effect sensor")
    ->set_sort_order(1350);
  ConfigItem(di4_gpio_config)
    ->set_title("GPIO for RESET button")
    ->set_description("GPIO number connected to RESET button")
    ->set_sort_order(1400);
  ConfigItem(di4_dtime_config)
    ->set_title("Debounce Time for RESET button")
    ->set_description("Debounce time in ms for RESET button")
    ->set_sort_order(1450);

  /* Get data from saved values or default paramters */
  const float gypsy_circum = gypsy_circum_config->get_value();
  const int   up_delay     = up_delay_config->get_value();
  const int   down_delay   = down_delay_config->get_value();
  const int   di1_gpio     = di1_gpio_config->get_value();
  const int   di1_dtime    = di1_dtime_config->get_value();
  const int   di2_gpio     = di2_gpio_config->get_value();
  const int   di2_dtime    = di2_dtime_config->get_value();
  const int   di3_gpio     = di3_gpio_config->get_value();
  const int   di3_dtime    = di3_dtime_config->get_value();
  const int   di4_gpio     = di4_gpio_config->get_value();
  const int   di4_dtime    = di4_dtime_config->get_value();

  /* Get last saved chain length from disk */
  Preferences prefs;
  prefs.begin("chain", true);  // true = lecture seule
  float saved_length = prefs.getFloat("length", 0.0);
  prefs.end();

  /* Digital inputs */
  auto* di1_input = new DigitalInputChange(di1_gpio, INPUT, CHANGE, "/di1/digital_input");
  auto* di1_debounce = new DebounceInt(di1_dtime, "/di1/debounce");
  auto* di2_input = new DigitalInputChange(di2_gpio, INPUT, CHANGE, "/di2/digital_input");
  auto* di2_debounce = new DebounceInt(di2_dtime, "/di2/debounce");
  auto* di3_input = new DigitalInputChange(di3_gpio, INPUT, CHANGE, "/di3/digital_input");
  auto* di3_debounce = new DebounceInt(di3_dtime, "/di3/debounce");
  auto* di4_input = new DigitalInputChange(di4_gpio, INPUT, CHANGE, "/di4/digital_input");
  auto* di4_debounce = new DebounceInt(di4_dtime, "/di4/debounce");

  /**
   * An Integrator<int, float> called "accumulator" adds up all the counts it
   * receives (which are ints) and multiplies each count by gypsy_circum, which
   * is the amount of chain, in meters, that is moved by each revolution of the
   * windlass. (Since gypsy_circum is a float, the output of this transform must
   * be a float, which is why we use Integrator<int, float>). It can be
   * configured in the Config UI at accum_config_path.
   */
  //String accum_config_path = "/accumulator/circum";
  auto* accumulator =
      new Integrator<float, float>(gypsy_circum, saved_length, "/accumulator/circum");

  /* Observable direction ("up", "down" or "free fall") */
  auto* direction = new ObservableValue<String>("free fall");
  direction->connect_to(
    new SKOutputString("navigation.anchor.chainDirection", "/chain/direction") );

  /**
   * There is no path for the amount of anchor rode deployed in the current
   * Signal K specification. By creating an instance of SKMetaData, we can send
   * a partial or full defintion of the metadata that other consumers of Signal
   * K data might find useful. (For example, Instrument Panel will benefit from
   * knowing the units to be displayed.) The metadata is sent only the first
   * time the data value is sent to the server.
   */
  SKMetadata* metadata = new SKMetadata();
  metadata->units_ = "m";
  metadata->description_ = "Anchor Rode Deployed";
  metadata->display_name_ = "Rode Deployed";
  metadata->short_name_ = "Rode Out";

  /**
   * chain_counter is connected to accumulator, which is connected to an
   * SKOutputNumber, which sends the final result to the indicated path on the
   * Signal K server. (Note that each data type has its own version of SKOutput:
   * SKOutputNumber for floats, SKOutputInt, SKOutputBool, and SKOutputString.)
   */
  String sk_path = "navigation.anchor.rodeDeployed";
  String sk_path_config_path = "/rodeDeployed/sk";
  auto sk_output = new SKOutputFloat(sk_path, sk_path_config_path, metadata);
  accumulator->connect_to(sk_output);

  /* Publish sk data every seconds */
  auto* sk_timer = new RepeatSensor<bool>(1000, [direction, accumulator] () -> bool {
    accumulator->notify();
    direction->notify();
    return true;
  });

  /* Save the chain length function */
  auto save_chain_length = [accumulator]() {
    float current_length = accumulator->get();
    ESP_LOGI(__FILE__, "Longueur de %f sauvegardée.", current_length);
    Preferences prefs;
    prefs.begin("chain", false);  // false = écriture
    prefs.putFloat("length", current_length);
    prefs.end();
  };

  /* React to UP action */
  auto* up_handler = new LambdaConsumer<int>( [up_delay, delayptr, direction](int input) {
    ESP_LOGI(__FILE__, "Bouton UP Changes");
    if (delayptr != nullptr) { 
      event_loop()->remove(delayptr);
      delayptr=nullptr;
    }
    if (input == 1) {
      ESP_LOGI(__FILE__, "Bouton UP ON => Up");
      direction->set("up");
    } else {
      ESP_LOGI(__FILE__, "Bouton UP OFF => Free fall");
      delayptr = event_loop()->onDelay(up_delay, [delayptr, direction]() {
        direction->set("free fall");
        delayptr=nullptr;
      });
    }
  });
  di1_input->connect_to(di1_debounce)->connect_to(up_handler);

  /* React to DOWN action */
  auto* down_handler = new LambdaConsumer<int>( [down_delay, delayptr, direction](int input) {
    if (delayptr != nullptr) { 
      event_loop()->remove(delayptr);
      delayptr=nullptr;
    }
    if (input == 1) {
      ESP_LOGI(__FILE__, "Bouton DOWN ON => Down");
      direction->set("down");
    } else {
      ESP_LOGI(__FILE__, "Bouton DOWN OFF => Free fall");
      delayptr = event_loop()->onDelay(down_delay, [delayptr, direction]() {
        direction->set("free fall");
        delayptr=nullptr;
      });
    }
  });
  di2_input->connect_to(di2_debounce)->connect_to(down_handler);

  /* React to COUNTER action */
  auto* counter_handler = new LambdaConsumer<int>( [direction, save_chain_length, accumulator](int input) {
    if (input == 1) {
      ESP_LOGI(__FILE__, "Bouton Counter Rise");
      if (direction->get() == "up") {
        accumulator->set(-1);
        ESP_LOGI(__FILE__, "Décrémenté");
      } else {
        accumulator->set(1);
        ESP_LOGI(__FILE__, "Incrémenté");
      }
      save_chain_length();
    }
  });
  di3_input->connect_to(di3_debounce)->connect_to(counter_handler);

  /* React to RESET action */
  auto* reset_handler = new LambdaConsumer<int>( [accumulator, save_chain_length](int input) {
    if (input == 1) {
      accumulator->reset();
      accumulator->set(0);
      ESP_LOGI(__FILE__, "Longueur réinitialisée à 0");
      save_chain_length();
    }
  });
  di4_input->connect_to(di4_debounce)->connect_to(reset_handler);
}

/* The loop function is called in an endless loop during program execution.
   It simply calls `app.tick()` which will then execute all events needed. */
void loop() {
  event_loop()->tick();
}
