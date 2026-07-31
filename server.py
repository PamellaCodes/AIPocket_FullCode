from flask import Flask, request, send_file
from openai import OpenAI
import tempfile
import os
import datetime

app = Flask(__name__)

client = OpenAI(api_key="API_KEY")


def save_test_log(stt_text, ai_answer):
    with open("milestone1_log.txt", "a", encoding="utf-8") as f:
        f.write(f"\n[{datetime.datetime.now()}]\n")
        f.write(f"STT: {stt_text}\n")
        f.write(f"AI : {ai_answer}\n")


def needs_web_search(text):
    text = text.lower()
    keywords = [
        "saat ini", "sekarang", "hari ini", "terbaru",
        "presiden", "cuaca", "harga", "berita",
        "jadwal", "kurs", "update", "tahun ini",
        "cari jurnal", "link"
    ]
    return any(keyword in text for keyword in keywords)


def ask_ai(text):
    if needs_web_search(text):
        response = client.responses.create(
            model="gpt-4o-mini",
            tools=[{"type": "web_search"}],
            input=f"""
Jawab pertanyaan berikut dalam bahasa Indonesia.
Gunakan web search jika perlu info terbaru.
Jawab singkat, maksimal 2 kalimat.

Pertanyaan: {text}
"""
        )
        return response.output_text.strip()

    response = client.chat.completions.create(
        model="gpt-4o-mini",
        messages=[
            {
                "role": "system",
                "content": "Kamu adalah AI Pocket. Jawab singkat, natural, dan padat dalam bahasa Indonesia, maksimal 2 kalimat."
            },
            {
                "role": "user",
                "content": text
            }
        ]
    )
    return response.choices[0].message.content.strip()


@app.route("/transcribe", methods=["POST"])
def transcribe():
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmp.write(request.data)
        tmp_path = tmp.name

    # Simpan audio asli dari ESP32 untuk debugging
    with open("debug.wav", "wb") as f:
        f.write(request.data)

    try:
        with open(tmp_path, "rb") as audio_file:
            transcript = client.audio.transcriptions.create(
                model="whisper-1",
                file=audio_file,
                language="id"
            )

        text = transcript.text.strip()
        print(f"STT: {text}")

        answer = ask_ai(text)
        print(f"AI: {answer}")

        save_test_log(text, answer)

        speech_file = "answer.wav"

        with client.audio.speech.with_streaming_response.create(
            model="gpt-4o-mini-tts",
            voice="alloy",
            input=answer,
            response_format="wav"
        ) as response:

            response.stream_to_file(speech_file)

        return "OK"

    except Exception as e:
        print("ERROR:", e)
        return "Terjadi error di server.", 500

    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)

@app.route("/speak", methods=["GET"])
def speak():
    speech_file = "answer.wav"

    if not os.path.exists(speech_file):
        return "Audio not ready", 404

    return send_file(
        speech_file,
        mimetype="audio/wav"
    )

if __name__ == "__main__":
    print("Server ready!")
    app.run(host="0.0.0.0", port=5000)


# Running: python3 server.py