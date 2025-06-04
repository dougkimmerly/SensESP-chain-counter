#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/debounce.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include <Preferences.h>

using namespace sensesp;

#define GPIO_HALMET_DI1 23
#define GPIO_HALMET_DI2 25
#define GPIO_HALMET_DI3 27
#define GPIO_HALMET_DI4 26

const uint8_t UP_PIN =      GPIO_HALMET_DI1;
const uint8_t DOWN_PIN =    GPIO_HALMET_DI2;
const uint8_t COUNTER_PIN = GPIO_HALMET_DI3;
const uint8_t BUTTON_PIN =  GPIO_HALMET_DI4;
const float gypsy_circum = 0.25;

bool up = false;

Preferences prefs;

/**
 * This example illustrates an anchor chain counter. Note that it
 * doesn't distinguish between chain being let out and chain being
 * taken in, so the intended usage is this: Press the button to make
 * sure the counter is at 0.0. Let out chain until the counter shows
 * the amount you want out. Once the anchor is set, press the button
 * again to reset the counter to 0.0. As you bring the chain in, the
 * counter will show how much you have brought in. Press the button
 * again to reset the counter to 0.0, to be ready for the next anchor
 * deployment.
 *
 * A bi-directional chain counter is possible, but this is not one.
 */

void setup() {
  SetupLogging();

  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("ChainCounter")
                    ->get_app();

  prefs.begin("chain", true);  // true = lecture seule
  float saved_length = prefs.getFloat("length", 0.0);
  prefs.end();

  /**
   * An Integrator<int, float> called "accumulator" adds up all the counts it
   * receives (which are ints) and multiplies each count by gypsy_circum, which
   * is the amount of chain, in meters, that is moved by each revolution of the
   * windlass. (Since gypsy_circum is a float, the output of this transform must
   * be a float, which is why we use Integrator<int, float>). It can be
   * configured in the Config UI at accum_config_path.
   */
  String accum_config_path = "/accumulator/circum";
  auto* accumulator =
      new Integrator<float, float>(gypsy_circum, saved_length, accum_config_path);

  ConfigItem(accumulator)
      ->set_title("Gypsy circum")
      ->set_description("Gypsy circum in m")
      ->set_sort_order(1000);

  /* Save the chain length function */
  auto save_chain_length = [accumulator]() {
    float current_length = accumulator->get();
    ESP_LOGI(__FILE__, "Longueur de %f sauvegardée.", current_length);
    Preferences prefs;
    prefs.begin("chain", false);  // false = écriture
    prefs.putFloat("length", current_length);
    prefs.end();
  };

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

  auto* sk_timer = new RepeatSensor<bool>(1000, [accumulator] () -> bool {
    accumulator->notify();
    return true;
  });



// Détection UP_PIN (avec debounce)
auto* up_input = new DigitalInputChange(UP_PIN, INPUT_PULLUP, CHANGE, "/up/read_delay");
auto* up_debounce = new DebounceInt(15, "/up/debounce");
auto* up_handler = new LambdaConsumer<int>( [](int input) {
  ESP_LOGI(__FILE__, "Bouton UP Changes");
  if (input == 1) {
    ESP_LOGI(__FILE__, "Bouton UP ON => Up");
    up =true;
  } else {
    ESP_LOGI(__FILE__, "Bouton UP OFF => Down");
    up = false;
  }
});
up_input->connect_to(up_debounce)->connect_to(up_handler);

// Détection DOWN_PIN (avec debounce)
auto* down_input = new DigitalInputChange(DOWN_PIN, INPUT, CHANGE, "/down/read_delay");
auto* down_debounce = new DebounceInt(15, "/down/debounce");
auto* down_handler = new LambdaConsumer<int>( [](int input) {
  if (input == 1) {
    ESP_LOGI(__FILE__, "Bouton Down Rise => Down");
    up = false;
  }
});
down_input->connect_to(down_debounce)->connect_to(down_handler);

// Détection COUNTER_PIN (avec debounce)
auto* counter_input = new DigitalInputChange(COUNTER_PIN, INPUT, CHANGE, "/counter/read_delay");
auto* counter_debounce = new DebounceInt(15, "/counter/debounce");


auto* counter_handler = new LambdaConsumer<int>( [save_chain_length, accumulator](int input) {
  if (input == 1) {
  ESP_LOGI(__FILE__, "Bouton Counter Rise");
  
  if (up) {
    accumulator->set(-1);
    //chain_length = chain_length - gypsy_circum;
    ESP_LOGI(__FILE__, "Décrémenté");
  } else {
    accumulator->set(1);
    //accumulator->notify();
    //chain_length = chain_length + gypsy_circum;
    ESP_LOGI(__FILE__, "Incrémenté");
  }
  save_chain_length();
  }
});
counter_input->connect_to(counter_debounce)->connect_to(counter_handler);


  /**
   * DigitalInputChange monitors a physical button connected to BUTTON_PIN.
   * Because its interrupt type is CHANGE, it will emit a value when the button
   * is pressed, and again when it's released, but that's OK - our
   * LambdaConsumer function will act only on the press, and ignore the release.
   * DigitalInputChange looks for a change every read_delay ms, which can be
   * configured at read_delay_config_path in the Config UI.
   */
  String read_delay_config_path = "/button_watcher/read_delay";
  auto* button_watcher = new DigitalInputChange(BUTTON_PIN, INPUT_PULLUP, CHANGE,
                                                read_delay_config_path);

  /**
   * Create a DebounceInt to make sure we get a nice, clean signal from the
   * button. Set the debounce delay period to 15 ms, which can be configured at
   * debounce_config_path in the Config UI.
   */
  int debounce_delay = 15;
  String debounce_config_path = "/debounce/delay";
  auto* debounce = new DebounceInt(debounce_delay, debounce_config_path);

  /**
   * When the button is pressed (or released), it will call the lambda
   * expression (or "function") that's called by the LambdaConsumer. This is the
   * function - notice that it calls reset() only when the input is 1, which
   * indicates a button press. It ignores the button release. If your button
   * goes to GND when pressed, make it "if (input == 0)".
   */
  auto reset_function = [accumulator, save_chain_length](int input) {
    if (input == 1) {
      accumulator->reset();
      accumulator->set(0);
      ESP_LOGI(__FILE__, "Longueur réinitialisée à 0");
      save_chain_length();
    }
  };

  /**
   * Create the LambdaConsumer that calls reset_function, Because
   DigitalInputChange
   * outputs an int, the version of LambdaConsumer we need is
   LambdaConsumer<int>.
  */
  auto* button_consumer = new LambdaConsumer<int>(reset_function);

  /* Connect the button_watcher to the debounce to the button_consumer. */
  button_watcher->connect_to(debounce)->connect_to(button_consumer);
}

// The loop function is called in an endless loop during program execution.
// It simply calls `app.tick()` which will then execute all events needed.
void loop() {
  event_loop()->tick();
}
