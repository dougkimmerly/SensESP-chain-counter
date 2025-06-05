#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/debounce.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include <Preferences.h>

using namespace sensesp;

/* Define GPIOs */
const int GPIO_DI1 = 23; //DIGITAL 1 in HALMET for UP relay
const int GPIO_DI2 = 25; //DIGITAL 2 in HALMET for DOWN relay
const int GPIO_DI3 = 27; //DIGITAL 3 in HALMET for CCOUNTER
const int GPIO_DI4 = 26; //DIGITAL 4 in HALMET for RESET button
const float gypsy_circum = 0.25;
static reactesp::Event* delayptr = nullptr;

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
  /* Prepare application */
  SetupLogging();
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("ChainCounter")
                    ->get_app();

  /* Get last saved chain length from disk */
  Preferences prefs;
  prefs.begin("chain", true);  // true = lecture seule
  float saved_length = prefs.getFloat("length", 0.0);
  prefs.end();

  /* Digital inputs */
  auto* di1_input = new DigitalInputChange(GPIO_DI1, INPUT_PULLUP, CHANGE, "/di1/digital_input");
  auto* di1_debounce = new DebounceInt(15, "/di1/debounce");
  auto* di2_input = new DigitalInputChange(GPIO_DI2, INPUT_PULLUP, CHANGE, "/di2/digital_input");
  auto* di2_debounce = new DebounceInt(15, "/di2/debounce");
  auto* di3_input = new DigitalInputChange(GPIO_DI3, INPUT_PULLUP, CHANGE, "/di3/digital_input");
  auto* di3_debounce = new DebounceInt(15, "/di3/debounce");
  auto* di4_input = new DigitalInputChange(GPIO_DI4, INPUT_PULLUP, CHANGE, "/di4/digital_input");
  auto* di4_debounce = new DebounceInt(15, "/di4/debounce");

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

  /* Set Gypsy Circum in web interface */
  ConfigItem(accumulator)
      ->set_title("Gypsy circum")
      ->set_description("Gypsy circum in m")
      ->set_sort_order(1000);

  /* Observable direction ("up", "down" or "free fall") */
  auto* direction = new ObservableValue<String>("free_fall");
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

  /* Up pin */
  auto* up_handler = new LambdaConsumer<int>( [delayptr, direction](int input) {
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
      delayptr = event_loop()->onDelay(2000, [delayptr, direction]() {
        direction->set("free fall");
        delayptr=nullptr;
      });
    }
  });
  di1_input->connect_to(di1_debounce)->connect_to(up_handler);

  /* Détection DOWN_PIN (avec debounce) */
  auto* down_handler = new LambdaConsumer<int>( [delayptr, direction](int input) {
    if (delayptr != nullptr) { 
      event_loop()->remove(delayptr);
      delayptr=nullptr;
    }
    if (input == 1) {
      ESP_LOGI(__FILE__, "Bouton DOWN ON => Down");
      direction->set("down");
    } else {
      ESP_LOGI(__FILE__, "Bouton DOWN OFF => Free fall");
      delayptr = event_loop()->onDelay(2000, [delayptr, direction]() {
        direction->set("free fall");
        delayptr=nullptr;
      });
    }
  });
  di2_input->connect_to(di2_debounce)->connect_to(down_handler);

  /* Détection COUNTER_PIN (avec debounce) */
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

  /**
   * When the button is pressed (or released), it will call the lambda
   * expression (or "function") that's called by the LambdaConsumer. This is the
   * function - notice that it calls reset() only when the input is 1, which
   * indicates a button press. It ignores the button release. If your button
   * goes to GND when pressed, make it "if (input == 0)".
   */
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
