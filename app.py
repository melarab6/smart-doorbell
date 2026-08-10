from flask import Flask, request, jsonify, render_template, send_from_directory
from database import init_db, add_event, get_events
from pathlib import Path
from datetime import datetime
import uuid

app = Flask(__name__, template_folder="templates")
UPLOAD_FOLDER = Path(__file__).parent / "uploads"
UPLOAD_FOLDER.mkdir(exist_ok=True)

init_db()

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/api/events", methods=["POST"]) # recieves data from ESP!
def receive_event():

    event_type = request.form.get("event_type")
    if event_type not in ["motion", "doorbell"]:
        return jsonify({
            "success": False,
            "error": "Invalid event type"
        }), 400

    image = request.files.get("image")
    if image is None:
        return jsonify({
            "success": False,
            "error": "Image is required"
        }), 400

    image_filename = f"{uuid.uuid4().hex}.jpg"

    image_path = UPLOAD_FOLDER / image_filename

    image.save(image_path)

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    event_id = add_event(
        event_type,
        image_filename,
        timestamp
    )

    return jsonify({
        "success": True,
        "event_id": event_id
    }), 201

@app.route("/api/events", methods=["GET"])
def retrieve_events():
    events = get_events()

    return jsonify({
        "success": True,
        "events": events
    }), 200

@app.route("/uploads/<filename>")
def uploaded_image(filename):
    return send_from_directory(UPLOAD_FOLDER, filename)

if __name__=='__main__':
    app.run(debug=True)