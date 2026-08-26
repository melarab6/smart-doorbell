"""Local object detection for doorbell images."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from ultralytics import YOLO


# Load the lightweight model once when Flask starts.
model = YOLO("yolo26n.pt")


def analyze_image(image_path):
    """Analyze an image, save an annotated copy, and return structured AI data."""
    image_path = Path(image_path)
    annotated_filename = f"annotated_{image_path.name}"
    annotated_path = image_path.with_name(annotated_filename)

    print(f"AI: analyzing {image_path.name}")
    results = model.predict(
        source=str(image_path),
        conf=0.50,
        save=False,
        verbose=False,
    )

    detections = []

    for result in results:
        for box in result.boxes:
            confidence = float(box.conf[0])
            if confidence < 0.50:
                continue

            class_id = int(box.cls[0])
            coordinates = [round(float(value), 2) for value in box.xyxy[0].tolist()]
            detections.append({
                "label": result.names[class_id],
                "confidence": round(confidence, 2),
                "bbox": coordinates,
            })

            print(
                f"AI: detected {result.names[class_id]} "
                f"{round(confidence * 100)}%"
            )

    create_annotated_image(image_path, annotated_path, detections)
    person_count = sum(
        1 for detection in detections
        if detection["label"].lower() == "person"
    )

    print("AI: annotated image saved")
    print("AI: analysis complete")

    return {
        "detections": detections,
        "person_detected": person_count > 0,
        "person_count": person_count,
        "annotated_filename": annotated_filename,
        "ai_summary": build_ai_summary(detections),
    }


def create_annotated_image(image_path, annotated_path, detections):
    """Draw detection boxes on a copy, leaving the uploaded image untouched."""
    with Image.open(image_path) as original:
        annotated = original.convert("RGB")

    draw = ImageDraw.Draw(annotated)
    font = ImageFont.load_default()

    for detection in detections:
        x1, y1, x2, y2 = detection["bbox"]
        label = detection["label"].replace("_", " ").title()
        label_text = f"{label} {round(detection['confidence'] * 100)}%"

        draw.rectangle((x1, y1, x2, y2), outline="#27f26a", width=3)
        text_box = draw.textbbox((x1, y1), label_text, font=font)
        text_height = text_box[3] - text_box[1]
        label_top = max(0, y1 - text_height - 8)
        label_box = draw.textbbox((x1 + 4, label_top + 4), label_text, font=font)
        draw.rectangle(
            (x1, label_top, label_box[2] + 4, label_box[3] + 4),
            fill="#27f26a",
        )
        draw.text((x1 + 4, label_top + 4), label_text, fill="#102117", font=font)

    annotated.save(annotated_path, format="JPEG", quality=92)


def build_ai_summary(detections):
    """Build a short deterministic sentence from detected object labels."""
    if not detections:
        return "No recognized objects detected"

    counts = {}
    for detection in detections:
        label = detection["label"].replace("_", " ").lower()
        counts[label] = counts.get(label, 0) + 1

    phrases = []
    for label, count in counts.items():
        if label == "person":
            name = "person" if count == 1 else "people"
        else:
            name = label if count == 1 else f"{label}s"

        article = "an" if label[:1] in "aeiou" else "a"
        phrases.append(f"{count} {name}" if count > 1 else f"{article} {name}")

    if len(phrases) == 1:
        object_text = phrases[0]
    else:
        object_text = ", ".join(phrases[:-1]) + f" and {phrases[-1]}"

    return f"{object_text.capitalize()} detected"
