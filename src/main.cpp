/* Bi-directional chain counter based on SensESP */
#include <memory>

#include "sensesp.h"
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
#include "sensesp/signalk/signalk_put_request_listener.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/types/position.h"
#include "ChainController.h"
#include "DeploymentManager.h"
#include "RetrievalManager.h"

using namespace sensesp;

bool ignore_input = true;

ChainController* chainController;
DeploymentManager* deploymentManager = nullptr;
RetrievalManager* retrievalManager = nullptr;

/* Prepare application */
void setup() {
  SetupLogging();
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("ChainCounter")
                    ->enable_ota("transport")
                    ->get_app();
  static reactesp::Event* buttonDelayPtr = nullptr;
  static reactesp::Event* commandDelayPtr = nullptr;

  /* Default values */
  float gypsy_circum_default = 0.25; //Default 0.25m
  float up_delay_default     = 2000; //Default 2000ms = 2s
  float down_delay_default   = 2000; //Default 2000ms = 2s
  float di1_gpio_default     = 23;   // UP Button
  float di1_dtime_default    = 15;   // Default 15 ms
  float di2_gpio_default     = 25;   // DOWN Button
  float di2_dtime_default    = 15;   // Default 15 ms
  float di3_gpio_default     = 27;   // Hall effect sensor
  float di3_dtime_default    = 15;   // Default 15 ms
  float di4_gpio_default     = 26;   // RESET Button
  float di4_dtime_default    = 15;   // Default 15 ms
  float upRelay_default      = 16;   // UP Relay
  float dnRelay_default      = 19;   // DOWN Relay
  float max_chain_default    = 80.0; // Default 80m


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
  String max_chain_config_path    = "/chain/max_length";
  String upRelay_config_path     = "/di5/gpio";
  String dnRelay_config_path     = "/di6/gpio"; 
  


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
  auto max_chain_config    = std::make_shared<NumberConfig>(max_chain_default,     max_chain_config_path    );
  auto upRelay_config      = std::make_shared<NumberConfig>(upRelay_default,       upRelay_config_path      );
  auto dnRelay_config      = std::make_shared<NumberConfig>(dnRelay_default,       dnRelay_config_path      );
  
  
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
  ConfigItem(upRelay_config)
    ->set_title("GPIO for UP relay")
    ->set_description("GPIO number connected to UP Button relay")
    ->set_sort_order(1125);
  ConfigItem(di1_dtime_config)
    ->set_title("Debounce Time for UP button")
    ->set_description("Debounce time in ms for UP Button relay")
    ->set_sort_order(1150);
  ConfigItem(di2_gpio_config)
    ->set_title("GPIO for DOWN button")
    ->set_description("GPIO number connected to DOWN Button relay")
    ->set_sort_order(1200);
  ConfigItem(dnRelay_config)
    ->set_title("GPIO for DOWN relay")
    ->set_description("GPIO number connected to DOWN Button relay")
    ->set_sort_order(1135);   
  ConfigItem(di2_dtime_config)
    ->set_title("Debounce Time for DOWN button")
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
  ConfigItem(max_chain_config)
    ->set_title("Max chain length")
    ->set_description("Maximum length of the chain in meters")
    ->set_sort_order(1500);

  /* Get data from saved values or default parameters */
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
  const int   upRelay      = upRelay_config->get_value();
  const int   dnRelay      = dnRelay_config->get_value();
  const float max_chain    = max_chain_config->get_value();
 

  /* Get last saved chain length from disk */
  Preferences prefs;
  prefs.begin("chain", true);  // true = read only
  float saved_length = prefs.getFloat("length", 0.0);
  prefs.end();

  ESP_LOGD(__FILE__, "the saved chain length is %f", saved_length );

  /* Digital inputs */
  auto* di1_input = new DigitalInputChange(di1_gpio, INPUT_PULLDOWN, CHANGE, "/di1/digital_input");
  auto* di1_debounce = new DebounceInt(di1_dtime, "/di1/debounce");
  auto* di2_input = new DigitalInputChange(di2_gpio, INPUT_PULLDOWN, CHANGE, "/di2/digital_input");
  auto* di2_debounce = new DebounceInt(di2_dtime, "/di2/debounce");
  auto* di3_input = new DigitalInputChange(di3_gpio, INPUT_PULLDOWN, CHANGE, "/di3/digital_input");
  auto* di3_debounce = new DebounceInt(di3_dtime, "/di3/debounce");
  auto* di4_input = new DigitalInputChange(di4_gpio, INPUT_PULLDOWN, CHANGE, "/di4/digital_input");
  auto* di4_debounce = new DebounceInt(di4_dtime, "/di4/debounce");
  
  /*Digital Outputs*/
  int upRelayPin = (int)upRelay;
  int dnRelayPin = (int)dnRelay;

  pinMode(upRelayPin, OUTPUT);
  pinMode(dnRelayPin, OUTPUT);  
  digitalWrite(upRelay, LOW); // Relay off
  digitalWrite(dnRelay, LOW); // Relay off


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
   * a partial or full definition of the metadata that other consumers of Signal
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

  /* Publish sk data every second */
  auto* sk_timer = new RepeatSensor<bool>(11000, [direction, accumulator] () -> bool {
    accumulator->notify();
    direction->notify();
    return true;
  });

  /* Save the chain length function */
  auto save_chain_length = [accumulator]() {
    if(ignore_input) {
      return;
    }
    float current_length = accumulator->get();
    // ESP_LOGD(__FILE__, "Deployed chain of %f saved to nvm.", current_length);
    Preferences prefs;
    prefs.begin("chain", false);  // false = writeable
    prefs.putFloat("length", current_length);
    prefs.end();
  };

  /* React to UP action */
  auto* up_handler = new LambdaConsumer<int>( [up_delay, direction](int input) {
    ESP_LOGD(__FILE__, "Button UP Changed");
    if(ignore_input) {
      return;
    }
    if (buttonDelayPtr != nullptr) {
      event_loop()->remove(buttonDelayPtr);
      buttonDelayPtr=nullptr;
    }
    if (input == 0) {
      ESP_LOGD(__FILE__, "Button UP ON => Up");
      direction->set("up");
    } else {
      ESP_LOGD(__FILE__, "Button UP OFF => Free fall");
      buttonDelayPtr = event_loop()->onDelay(up_delay, [direction]() {
        direction->set("free fall");
        buttonDelayPtr=nullptr;
      });
    }
  });
  di1_input->connect_to(di1_debounce)->connect_to(up_handler);

  /* React to DOWN action */
  auto* down_handler = new LambdaConsumer<int>( [down_delay, direction](int input) {
    ESP_LOGD(__FILE__, "Button DOWN Changed");
    if(ignore_input) {
      return;
    }
    if (buttonDelayPtr != nullptr) {
      event_loop()->remove(buttonDelayPtr);
      buttonDelayPtr=nullptr;
    }
    if (input == 0) {
      ESP_LOGD(__FILE__, "Button DOWN ON => Down");
      direction->set("down");
    } else {
      ESP_LOGD(__FILE__, "Button DOWN OFF => Free fall");
      buttonDelayPtr = event_loop()->onDelay(down_delay, [direction]() {
        direction->set("free fall");
        buttonDelayPtr=nullptr;
      });
    }
  });
  di2_input->connect_to(di2_debounce)->connect_to(down_handler);

  /* React to COUNTER action */
  auto* counter_handler = new LambdaConsumer<int>( [
    gypsy_circum, max_chain, direction, save_chain_length, accumulator
     ](int input) {
    if(ignore_input) {
      return;
    }
      // ESP_LOGD(__FILE__, "the input change is %d", input);
    if (input == 1) {
        float current_value = accumulator->get();

      // ESP_LOGD(__FILE__, "The Gypsy is turning");
      if (direction->get() == "up") {
        if(current_value - gypsy_circum < 0) {
            // ESP_LOGD(__FILE__, "Already at 0m of Chain ");
        } else {
            accumulator->set(-1);
            // ESP_LOGD(__FILE__, "Decrement Deployed Chain");
        }

      } else {
        if(current_value + gypsy_circum > max_chain) {
            // ESP_LOGD(__FILE__, "Already at the Max %fm Chain Length", max_chain);
        } else {
            accumulator->set(1);
            // ESP_LOGD(__FILE__, "Increment Deployed Chain");
        }
        
      }
      save_chain_length();
    }
  });
  di3_input->connect_to(di3_debounce)->connect_to(counter_handler);

  /* React to RESET action */
  auto* reset_handler = new LambdaConsumer<int>( [accumulator, save_chain_length](int input) {
    if(ignore_input) {  
      return;
    }
    if (input == 1) {
      accumulator->reset();
      accumulator->set(0);
      ESP_LOGD(__FILE__, "Deployed chain reset to 0");
      save_chain_length();
    }
  });
  di4_input->connect_to(di4_debounce)->connect_to(reset_handler);


  // Set up a listener to respond to requested resets of the chain length
  auto* reset_listener = new IntSKPutRequestListener("navigation.anchor.rodeDeployed");
  reset_listener->connect_to(reset_handler);



//////////////////////////////////////////////////////////////////////////
//      Windlass Control Section
//////////////////////////////////////////////////////////////////////////


  float min_length = 2.0;  // stop 2 meters before anchor is fully up
  float stop_before_max = max_chain - 5.0;  // stop 5 meters before max

  chainController = new ChainController(
    min_length, 
    max_chain, 
    stop_before_max, 
    accumulator, 
    dnRelayPin,
    upRelayPin
  );


// Create a feedback connection from the accumulator to the chainController
// so that it can monitor the current chain length and stop movement at limits
  auto* feedback = new sensesp::LambdaConsumer<float>(
      [&](float pos){ chainController->control(pos); }
  );
  accumulator->connect_to(feedback);

// initialize up and down speeds from preferences
  chainController->loadSpeedsFromPrefs();

  deploymentManager = new DeploymentManager(
    chainController
  );

  retrievalManager = new RetrievalManager(
    chainController
  );

  auto slackChain = new SKOutputFloat(
    "navigation.anchor.chainSlack",
    "/slack/sk",
    new SKMetadata("m", "Anchor Chain Slack", "Chain Slack", "Slack")
  );
  chainController->getHorizontalSlackObservable()->connect_to(slackChain);
  auto* slack_update_timer = new RepeatSensor<bool>(500, []() -> bool {
    chainController->calculateAndPublishHorizontalSlack();
    return true;
  });



// Set up SKOutput so that we can then receive anchor commands
// on this path
  auto* anchor_command = new ObservableValue<String>("idle");
  anchor_command->connect_to(
  new SKOutputString("navigation.anchor.command", "/anchorCommand/sk") );

  // Set completion callbacks for auto commands to reset anchor_command to idle
  deploymentManager->setCompletionCallback([anchor_command]() {
    anchor_command->set("idle");
    ESP_LOGI(__FILE__, "autoDrop completed, command set to idle");
  });
  retrievalManager->setCompletionCallback([anchor_command]() {
    anchor_command->set("idle");
    ESP_LOGI(__FILE__, "autoRetrieve completed, command set to idle");
  });

  auto* sk_timer2 = new RepeatSensor<bool>(11000, [anchor_command] () -> bool {
    anchor_command->notify();
    return true;
  });

  auto* command_listener = new StringSKPutRequestListener("navigation.anchor.command");
  
  /*
  This is the main command handler for the windlass commands
  As you create new String commands to be PUT to this path
  you can add the controls here to respond to them.
  The first thing that happens when a command is received is to
  stop any current movement of the windlass. And if there is
  a command timeout in progress, that is also cancelled.

  Currently setup commands:
    "drop"       - starts lowering the anchor until depth + 4m is reached 
                    this is meant as the initial drop command
    "raiseXX"    - starts raising the anchor by XX meters, e.g. "raise10" or "raise 10"
                    raises the anchor by 10 meters
    "lowerXX"    - starts lowering the anchor by XX meters e.g. "lower10" or "lower 10" 
                    lowers the anchor by 10 meters                
    "STOP"       - stops any movement in progress (since every command stops movement first,
                    anything not defined here will just stop the windlass)

  */
  command_listener->connect_to(new LambdaConsumer<String>( [
    dnRelayPin, upRelayPin, accumulator, anchor_command
     ](String input) {

      ESP_LOGI(__FILE__, "Command received is %s", input.c_str());
      if (chainController->isActive()) {
          chainController->stop();
      }
      if(commandDelayPtr != nullptr) {
        event_loop()->remove(commandDelayPtr);
        commandDelayPtr=nullptr;
      }
    float chainStart = accumulator->get();
    if(input == "drop") {
        ESP_LOGI(__FILE__, "DROP command received");
        anchor_command->set("drop");
        float drop_depth = chainController->getDepthListener()->get() + 4.0; // add 4m to the depth for slack chain on bottom
        chainController->lowerAnchor(drop_depth);
        unsigned long moveTime = chainController->getTimeout();
         commandDelayPtr = event_loop()->onDelay(moveTime, [moveTime, anchor_command]() {
          ESP_LOGI(__FILE__, "movement timeout reached, stopping chain %1u s", moveTime);
          chainController->stop();
          anchor_command->set("idle");
          commandDelayPtr=nullptr;
        });
    }
    if (input.startsWith("raise")) {
        int spaceIndex = input.indexOf(' ');
        float raise_amount = 0;

        if (spaceIndex != -1) {
            // handle "raise 10" (with space)
            String number_str = input.substring(spaceIndex + 1);
            raise_amount = number_str.toFloat();
        } else {
            // handle "raise10" (no space)
            String number_str = input.substring(5);
            raise_amount = number_str.toFloat();
        }

        ESP_LOGI(__FILE__, "Raising %.2f meters", raise_amount);
        anchor_command->set("raise");
        chainController->raiseAnchor(raise_amount);
        unsigned long moveTime = chainController->getTimeout();

        commandDelayPtr = event_loop()->onDelay(moveTime, [moveTime, anchor_command]() {
            ESP_LOGI(__FILE__, "movement timeout reached, stopping chain %1u s", moveTime);
            chainController->stop();
            anchor_command->set("idle");
            commandDelayPtr = nullptr;
        });
    }
    if (input.startsWith("lower")) {
        int spaceIndex = input.indexOf(' ');
        float lower_amount = 0;

        if (spaceIndex != -1) {
            // handle "lower 10" (with space)
            String number_str = input.substring(spaceIndex + 1);
            lower_amount = number_str.toFloat();
        } else {
            // handle "lower10" (no space)
            String number_str = input.substring(5);
            lower_amount = number_str.toFloat();
        }

        ESP_LOGI(__FILE__, "Lowering %.2f meters", lower_amount);
        anchor_command->set("lower");
        chainController->lowerAnchor(lower_amount);
        unsigned long moveTime = chainController->getTimeout();

        commandDelayPtr = event_loop()->onDelay(moveTime, [moveTime, anchor_command]() {
            ESP_LOGI(__FILE__, "movement timeout reached, stopping chain %1u s", moveTime);
            chainController->stop();
            anchor_command->set("idle");
            commandDelayPtr = nullptr;
        });
    }
    if(input.startsWith("autoDrop")) {
      float scopeRatio = 5.0;  // Default scope ratio

      // Extract numeric suffix if present (e.g., "autoDrop7" → 7.0)
      if (input.length() > 8) {  // "autoDrop" is 8 characters
          String suffix = input.substring(8);
          float parsedRatio = suffix.toFloat();
          if (parsedRatio > 0) {
              scopeRatio = parsedRatio;
          }
      }

      ESP_LOGI(__FILE__, "Starting autoDrop with scope ratio %.1f:1", scopeRatio);
      retrievalManager->stop();  // Stop retrieval if running
      anchor_command->set("autoDrop");
      deploymentManager->start(scopeRatio);
    }
    if(input == "autoRetrieve") {
      ESP_LOGI(__FILE__, "AUTO-RETRIEVE command received");
      deploymentManager->stop();  // Stop deployment if running (its monitoring interferes with retrieval)
      anchor_command->set("autoRetrieve");
      retrievalManager->start();
    }
    if (input == "stop") {
        chainController->stop();
        deploymentManager->stop();
        retrievalManager->stop();
        anchor_command->set("idle");
        commandDelayPtr = nullptr;
    }
    





  }));

///////////////////////////////////////////////////////////////////////////
// End of Windlass Control Section
///////////////////////////////////////////////////////////////////////////

  // Startup delay to ignore input changes while system stabilizes

    int iStateCounter = digitalRead(di3_gpio);
    ESP_LOGD(__FILE__, "Initial di3_gpio state: %d", iStateCounter);
    int iStateUP = digitalRead(upRelayPin);
    ESP_LOGD(__FILE__, "Initial UP button state: %d", iStateUP);
    int iStateDOWN = digitalRead(dnRelayPin);
    ESP_LOGD(__FILE__, "Initial DOWN button state: %d", iStateDOWN);

    event_loop()->onDelay(2000, []() {
      ignore_input = false; 
    });

  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}




/* The loop function is called in an endless loop during program execution.
   It simply calls `app.tick()` which will then execute all events needed. */
void loop() {
  event_loop()->tick();
}
