import pyttsx3
engine = pyttsx3.init()
def speak(text):
    engine.say(text)
    engine.runAndWait()
if __name__ == "__main__":
    text = input("Enter text to speak: ")
    speak(text)