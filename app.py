from flask import Flask, request, jsonify, render_template, send_from_directory
from database import init_db, add_event, get_events, delete_event, delete_all_events
from vision import analyze_image
from pathlib import Path
from datetime import datetime
import uuid

app = Flask(__name__, template_folder="templates")
UPLOAD_FOLDER = Path(__file__).parent / "uploads"
UPLOAD_FOLDER.mkdir(exist_ok=True)

init_db() #calls the function from database.py

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

    # AI errors should never prevent the ESP32 upload from succeeding.
    try:
        ai_result = analyze_image(image_path)
    except Exception as error:
        print(f"AI ERROR: {error}")
        ai_result = {
            "detections": [],
            "person_detected": False,
            "person_count": 0,
            "annotated_filename": None,
            "ai_summary": "No recognized objects detected",
        }

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        #calls the add event function from database.py
    event_id = add_event(
        event_type,
        image_filename,
        timestamp,
        ai_result["detections"],
        ai_result["annotated_filename"],
        ai_result["person_detected"],
        ai_result["person_count"],
        ai_result["ai_summary"],
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

@app.route("/api/events/<int:event_id>", methods=["DELETE"])
def remove_event(event_id):
    image_filenames = delete_event(event_id)

    if image_filenames is None:
        return jsonify({
            "success": False,
            "error": "Event not found"
        }), 404

    for image_filename in image_filenames:
        if image_filename:
            remove_image(image_filename)

    return jsonify({"success": True}), 200

@app.route("/api/events", methods=["DELETE"])
def remove_all_events():
    image_filenames = delete_all_events()

    for image_filename in image_filenames:
        remove_image(image_filename)

    return jsonify({
        "success": True,
        "deleted_count": len(image_filenames)
    }), 200

def remove_image(image_filename):
    """Remove an event image while keeping deletion safe inside uploads."""
    image_path = UPLOAD_FOLDER / Path(image_filename).name

    try:
        image_path.unlink()
    except FileNotFoundError:
        pass

@app.route("/uploads/<filename>")
def uploaded_image(filename):
    return send_from_directory(UPLOAD_FOLDER, filename)

@app.route("/api/health")
def health():
    return jsonify({
        "status": "online"
    }), 200

if __name__ == "__main__":
    # Disable Flask's second debug process so the YOLO model loads only once.
    app.run(host="0.0.0.0", port=5000, debug=True, use_reloader=False)
