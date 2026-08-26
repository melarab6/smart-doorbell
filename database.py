import sqlite3
import json
from pathlib import Path

DB_PATH = Path(__file__).parent / "doorbell.db" #this is the connection to the database. our database that stores the data is doorbell.db and this file creates the db


def init_db(): #function that initializes the database
    connection = sqlite3.connect(DB_PATH)

    connection.execute("""
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type TEXT NOT NULL,
            image_filename TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            detections TEXT NOT NULL DEFAULT '[]',
            annotated_image_filename TEXT,
            person_detected INTEGER NOT NULL DEFAULT 0,
            person_count INTEGER NOT NULL DEFAULT 0,
            ai_summary TEXT NOT NULL DEFAULT 'No recognized objects detected'
        )
    """)

    # Migrate databases created before object detection was added.
    columns = connection.execute("PRAGMA table_info(events)").fetchall()
    column_names = [column[1] for column in columns]
    migrations = {
        "detections": "TEXT NOT NULL DEFAULT '[]'",
        "annotated_image_filename": "TEXT",
        "person_detected": "INTEGER NOT NULL DEFAULT 0",
        "person_count": "INTEGER NOT NULL DEFAULT 0",
        "ai_summary": "TEXT NOT NULL DEFAULT 'No recognized objects detected'",
    }

    for column_name, column_definition in migrations.items():
        if column_name not in column_names:
            connection.execute(
                f"ALTER TABLE events ADD COLUMN {column_name} {column_definition}"
            )

    connection.commit()
    connection.close()

def add_event(
    event_type,
    image_filename,
    timestamp,
    detections=None,
    annotated_image_filename=None,
    person_detected=False,
    person_count=0,
    ai_summary="No recognized objects detected",
):
    connection = sqlite3.connect(DB_PATH)

    cursor = connection.execute(
        """
        INSERT INTO events (
            event_type, image_filename, timestamp, detections,
            annotated_image_filename, person_detected, person_count, ai_summary
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            event_type,
            image_filename,
            timestamp,
            json.dumps(detections or []),
            annotated_image_filename,
            int(person_detected),
            person_count,
            ai_summary,
        )
    )

    connection.commit()

    event_id = cursor.lastrowid

    connection.close()

    return event_id

def get_events(): #function that retrieves events from the database
    connection = sqlite3.connect(DB_PATH)

    connection.row_factory = sqlite3.Row

    rows = connection.execute(
        """
        SELECT id, event_type, image_filename, timestamp, detections,
               annotated_image_filename, person_detected, person_count,
               ai_summary
        FROM events
        ORDER BY id DESC
        """
    ).fetchall()

    connection.close()

    events = []
    for row in rows:
        event = dict(row)
        try:
            event["detections"] = json.loads(event["detections"] or "[]")
        except (json.JSONDecodeError, TypeError):
            event["detections"] = []
        event["person_detected"] = bool(event["person_detected"])
        events.append(event)

    return events


def delete_event(event_id):
    """Delete one event and return both image filenames, if it existed."""
    connection = sqlite3.connect(DB_PATH)

    row = connection.execute(
        """SELECT image_filename, annotated_image_filename
           FROM events WHERE id = ?""",
        (event_id,)
    ).fetchone()

    if row is None:
        connection.close()
        return None

    connection.execute("DELETE FROM events WHERE id = ?", (event_id,))
    connection.commit()
    connection.close()

    return list(row)


def delete_all_events():
    """Delete every event and return all original and annotated filenames."""
    connection = sqlite3.connect(DB_PATH)

    rows = connection.execute(
        "SELECT image_filename, annotated_image_filename FROM events"
    ).fetchall()
    connection.execute("DELETE FROM events")
    connection.commit()
    connection.close()

    return [filename for row in rows for filename in row if filename]
