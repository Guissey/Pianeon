#ifndef INDEX_H
#define INDEX_H

#include <string>
// #include <iostream>
#include <sstream>

std::string getHtmlPage(char* brightness, char* red, char* green, char* blue, bool showSustain) {
  std::stringstream htmlStream;
  const char* sustainCheckedProp = showSustain ? "true" : "false";
  
  htmlStream << R"__(
<!DOCTYPE html>
<html>
<head>
  <title>PianoLED</title>
  <style>
    html {
      font-size: min(6vw, 40px);
    }
    @media only screen and (max-device-width: 992px) and (orientation: landscape) {
      html {
        font-size: min(6vw, 30px);
      }
    }
    @media only screen and (min-device-width: 992px) {
      html {
        font-size: min(6vw, 18px);
      }
    }
    body {
      background-color: #202020;
      color: #fff;
      max-width: 600px;
      margin: auto;
      padding: 1rem;
    }
    button, input {
      font-size: 1rem;
    }
    h1.title {
      text-align: center;
    }
    .color-values {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
    }
    .color-values > div {
      flex: 1;
      text-align: center;
    }
    .color-values p {
      margin: 0 0 0.1rem 0;
    }
    #current-color {
      max-width: 2rem;
      height: 2rem;
      border-radius: 0.5rem;
      border: none;
    }
    .rgb-input {
      background-color: black;
      color: white;
      text-align: center;
      border-radius: 0.2rem;
      width: 2rem;
    }
    #palette {
      max-width: 70%;
      margin: 1rem auto;
      display: flex;
      flex-wrap: wrap;
      gap: 0.75rem;
    }
    #palette > button {
      width: 1.5rem;
      height: 1.5rem;
      border-radius: 0.3rem;
      border: none;
    }
    .brightness-title {
      display: flex;
      flex-wrap: wrap;
      align-items: baseline;
    }
    #brightness-value {
      font-weight: bold;
      text-align: center;
      flex: 1;
    }
    .brightness-container {
      width: 100%;
    }
    #brightness-slider {
      width: 100%;
    }
    #sustain {
      width: 1rem;
      height: 1rem;
    }
  </style>
</head>

<body>
  <h1 class="title">
    PianoLED
  </h1>

  <div>
    <h2>LED color:</h2>
    <div class="color-values">
      <div id="current-color"></div>
      <div>
        <p>Red</p>
        <input type="text" id="red-input" class="rgb-input" />
      </div>
      <div>
        <p>Green</p>
        <input type="text" id="green-input" class="rgb-input" />
      </div>
      <div>
        <p>Blue</p>
        <input type="text" id="blue-input" class="rgb-input" />
      </div>
    </div>
    <div id="palette">
      <button style="background-color: rgb(255, 255, 255)" onclick="postColor(255, 255, 255)" ></button>
    </div>
    <button onclick="postRandomColor()">
      Random color
    </button>

    <div>
      <div class="brightness-title">
        <h2>Brightness:</h2>
        <p id="brightness-value" ></p>
      </div>
      <div class="brightness-container">
        <input type="range" min="0" max="255" id="brightness-slider" oninput="updateBrightness()" onchange="postBrightness()" />
      </div>
    </div>

    <div>
      <h2>Sustain:</h2>
      <div>
        <input type="checkbox" id="sustain" name="sustain" onChange="postSustain()" />
        <label for="sustain">Show sustain on sides</label>
    </div>

  </div>
</body>

<script>
  const palette = document.getElementById("palette");
  const redInput = document.getElementById("red-input");
  const greenInput = document.getElementById("green-input");
  const blueInput = document.getElementById("blue-input");
  const currentColorIndicator = document.getElementById("current-color");

  var currentColor = {
    red: )__" << red << R"__(,
    green: )__" << green << R"__(,
    blue: )__" << blue << R"__(,
  };

  const updateColorValues = () => {
    redInput.value = currentColor.red;
    greenInput.value = currentColor.green;
    blueInput.value = currentColor.blue;
    currentColorIndicator.style = `background-color:rgb(${currentColor.red}, ${currentColor.green}, ${currentColor.blue});`;
  }

  updateColorValues();

  const colorValues = [0, 64, 128, 192, 255];
  const rgbCodeValues = [
    [4, 0, 0],
    [4, 1, 0],
    [4, 2, 0],
    [4, 3, 0],
    [4, 4, 0],
    [3, 4, 0],
    [2, 4, 0],
    [1, 4, 0],
    [0, 4, 0],
    [0, 4, 1],
    [0, 4, 2],
    [0, 4, 3],
    [0, 4, 4],
    [0, 3, 4],
    [0, 2, 4],
    [0, 1, 4],
    [0, 0, 4],
    [1, 0, 4],
    [2, 0, 4],
    [3, 0, 4],
  ];
  for (const rgbCodeValue of rgbCodeValues) {
    const paletteButton = document.createElement("button");
    const red = colorValues[rgbCodeValue[0]];
    const green = colorValues[rgbCodeValue[1]];
    const blue = colorValues[rgbCodeValue[2]];

    paletteButton.style = `background-color: rgb(${red}, ${green}, ${blue})`;
    paletteButton.onclick = () => postColor(red, green, blue);
  
    palette.appendChild(paletteButton);
  };
  
  const onBlurColorInput = () => {
    console.log(event.target);
  }

  for (input of [redInput, greenInput, blueInput]) {
    input.addEventListener("blur", async () => {
      if (redInput.value == currentColor.red
      && greenInput.value == currentColor.green
      && blueInput.value == currentColor.blue) {
        return;
      }

      currentColor = {
        red: redInput.value,
        green: greenInput.value,
        blue: blueInput.value,
      };
      updateColorValues();
      await postColor(currentColor.red, currentColor.green, currentColor.blue);
    });

    input.addEventListener("keydown", (event) => {
      const keyCode = event.keyCode;
      console.log(keyCode);
      if (keyCode == 13) {
        switch (event.target) {
          case redInput:
            greenInput.focus();
            break;
          case greenInput:
            blueInput.focus();
            break;
          default:
            event.target.blur();
            break;
        }
      } else if (
        keyCode != 9
        && keyCode != 8
        && keyCode != 46
        && (keyCode < 48 || keyCode > 57)
        && (keyCode < 96 || keyCode > 105)
      ) {
        event.preventDefault();
      }
    });
  }

  const postColor = async (red, green, blue) => {
    const body = { red, green, blue };
    console.log("New color: ", JSON.stringify(body));
    await fetch("color", {
      method: "POST",
      body: JSON.stringify(body),
    });
    currentColor = { red, green, blue };
    updateColorValues();
  }

  const postRandomColor = async () => {
    const red = getRandomByte();
    const green = getRandomByte();
    const blue = getRandomByte();
    await postColor(red, green, blue);
  }

  const getRandomByte = () => Math.floor(Math.random() * 255);

  const brightnessInput = document.getElementById("brightness-slider");
  const brightnessValue = document.getElementById("brightness-value");

  brightnessValue.innerHTML = )__" << brightness << R"__(;
  brightnessInput.value = brightnessValue.innerHTML;

  const updateBrightness = () => {
    brightnessValue.innerHTML = brightnessInput.value;
  }

  const postBrightness = async () => {
    const brightness = brightnessInput.value;
    console.log("New brightness: ", brightness);
    await fetch("brightness", {
      method: "POST",
      body: JSON.stringify({ brightness }),
    });
  }

  const sustainInput = document.getElementById("sustain");
  sustainInput.checked = )__" << sustainCheckedProp << R"__(;

  const postSustain = async () => {
    const sustain = sustainInput.checked;
    console.log("Request show sustain", sustain);
    await fetch("sustain", {
      method: "POST",
      body: JSON.stringify({ sustain }),
    });
  }
</script>
</html>
)__";

  return htmlStream.str();
}

#endif