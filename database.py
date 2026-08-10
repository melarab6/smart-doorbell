import sqlite3
from pathlib import Path

DB_PATH = Path(__file__).parent / "doorbell.db" #this is the connection to the database. our database that stores the data is doorbell.db and this file creates the db


def init_db():
    connection = sqlite3.connect(DB_PATH)

    connection.execute("""
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type TEXT NOT NULL,
            image_filename TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    """)
    connection.commit()
    connection.close()

def add_event(event_type, image_filename, timestamp):
    connection = sqlite3.connect(DB_PATH)

    cursor = connection.execute(
        """
        INSERT INTO events (event_type, image_filename, timestamp)
        VALUES (?, ?, ?)
        """,
        (event_type, image_filename, timestamp)
    )

    connection.commit()

    event_id = cursor.lastrowid

    connection.close()

    return event_id

def get_events():
    connection = sqlite3.connect(DB_PATH)

    connection.row_factory = sqlite3.Row

    rows = connection.execute(
        """
        SELECT id, event_type, image_filename, timestamp
        FROM events
        ORDER BY id DESC
        """
    ).fetchall()

    connection.close()

    return [dict(row) for row in rows]