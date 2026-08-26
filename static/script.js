// Load the latest doorbell events after the document is ready.
// Wait until the HTML page is ready.
document.addEventListener("DOMContentLoaded", () => {
    document.getElementById("clear-all-button").addEventListener("click", clearAllEvents);
    // Load the events immediately when the page opens.
    loadActivity();

    // Ask Flask for the newest events every 5 seconds.
    setInterval(loadActivity, 5000);
});
async function loadActivity() {
    const activityList = document.getElementById("activity-list");

    try {
        const response = await fetch("/api/events", {
            headers: { Accept: "application/json" }
        });

        if (!response.ok) {
            throw new Error(`Request failed with status ${response.status}`);
        }

        const data = await response.json();

        if (!data.success || !Array.isArray(data.events)) {
            throw new Error("The activity response was not valid.");
        }

        renderEvents(activityList, data.events);
    } catch (error) {
        console.error("Unable to load doorbell activity:", error);
        renderMessage(
            activityList,
            "Unable to load doorbell activity.",
            "Please refresh the page to try again."
        );
    } finally {
        activityList.setAttribute("aria-busy", "false");
    }
}

function renderEvents(container, events) {
    const clearAllButton = document.getElementById("clear-all-button");
    clearAllButton.hidden = events.length === 0;
    container.replaceChildren();

    if (events.length === 0) {
        renderMessage(
            container,
            "No activity recorded yet.",
            "Your doorbell events will appear here."
        );
        return;
    }

    const cards = document.createDocumentFragment();

    events.forEach((event) => {
        cards.appendChild(createEventCard(event));
    });

    container.appendChild(cards);
}

function createEventCard(event) {
    const eventInfo = getEventInfo(event.event_type);
    const card = document.createElement("article");
    card.className = `event-card event-card--${eventInfo.style}`;

    const imageWrap = document.createElement("div");
    imageWrap.className = "event-image-wrap";

    const image = document.createElement("img");
    image.className = "event-image";
    const displayedFilename = event.annotated_image_filename || event.image_filename;
    image.src = `/uploads/${encodeURIComponent(displayedFilename)}`;
    image.alt = `${eventInfo.label} capture`;
    image.loading = "lazy";
    image.addEventListener("error", () => {
        imageWrap.classList.add("image-unavailable");
    });

    const imageFallback = document.createElement("span");
    imageFallback.className = "image-fallback";
    imageFallback.textContent = "Image unavailable";

    imageWrap.append(image, imageFallback);

    const details = document.createElement("div");
    details.className = "event-details";

    const textGroup = document.createElement("div");

    const label = document.createElement("div");
    label.className = "event-label";

    const icon = document.createElement("span");
    icon.className = "event-icon";
    icon.setAttribute("aria-hidden", "true");
    icon.textContent = eventInfo.icon;

    const labelText = document.createElement("span");
    labelText.textContent = eventInfo.label;
    label.append(icon, labelText);

    const timestamp = document.createElement("p");
    timestamp.className = "event-time";
    timestamp.textContent = event.timestamp || "Time unavailable";

    const personStatus = document.createElement("p");
    personStatus.className = event.person_detected
        ? "person-status person-status--detected"
        : "person-status";
    if (event.person_count > 1) {
        personStatus.textContent = `${event.person_count} People Detected`;
    } else if (event.person_detected) {
        personStatus.textContent = "Person Detected";
    } else {
        personStatus.textContent = "No Person Detected";
    }

    const aiSummary = document.createElement("p");
    aiSummary.className = "ai-summary";
    aiSummary.textContent = event.ai_summary || "No recognized objects detected";

    const detectionSummary = createDetectionSummary(event.detections);

    const badge = document.createElement("span");
    badge.className = "event-type-badge";
    badge.textContent = eventInfo.badge;

    const cardActions = document.createElement("div");
    cardActions.className = "event-actions";

    const deleteButton = document.createElement("button");
    deleteButton.className = "delete-button";
    deleteButton.type = "button";
    deleteButton.textContent = "Delete";
    deleteButton.setAttribute("aria-label", `Delete ${eventInfo.label} from ${event.timestamp || "unknown time"}`);
    deleteButton.addEventListener("click", () => deleteEvent(event.id, card, deleteButton));

    cardActions.append(badge, deleteButton);

    textGroup.append(label, timestamp, personStatus, aiSummary, detectionSummary);
    details.append(textGroup, cardActions);
    card.append(imageWrap, details);

    return card;
}

function createDetectionSummary(detections) {
    const summary = document.createElement("div");
    summary.className = "detection-summary";

    if (!Array.isArray(detections) || detections.length === 0) {
        summary.textContent = "No objects detected";
        return summary;
    }

    const heading = document.createElement("span");
    heading.className = "detection-heading";
    heading.textContent = "Detected Objects";

    const list = document.createElement("ul");
    detections.forEach((detection) => {
        const item = document.createElement("li");
        const label = capitalizeLabel(detection.label);
        const confidence = Math.round(Number(detection.confidence) * 100);
        item.textContent = `${label} — ${confidence}%`;
        list.appendChild(item);
    });

    summary.append(heading, list);
    return summary;
}

function capitalizeLabel(label) {
    return String(label || "Object")
        .replaceAll("_", " ")
        .replace(/\b\w/g, (letter) => letter.toUpperCase());
}

async function deleteEvent(eventId, card, button) {
    if (!window.confirm("Delete this image? This cannot be undone.")) {
        return;
    }

    button.disabled = true;

    try {
        await sendDeleteRequest(`/api/events/${encodeURIComponent(eventId)}`);
        card.remove();

        const activityList = document.getElementById("activity-list");
        if (!activityList.querySelector(".event-card")) {
            document.getElementById("clear-all-button").hidden = true;
            renderMessage(activityList, "No activity recorded yet.", "Your doorbell events will appear here.");
        }
    } catch (error) {
        console.error("Unable to delete event:", error);
        window.alert("The image could not be deleted. Please try again.");
        button.disabled = false;
    }
}

async function clearAllEvents() {
    if (!window.confirm("Delete all images and activity history? This cannot be undone.")) {
        return;
    }

    const button = document.getElementById("clear-all-button");
    button.disabled = true;

    try {
        await sendDeleteRequest("/api/events");
        button.hidden = true;
        renderMessage(
            document.getElementById("activity-list"),
            "No activity recorded yet.",
            "Your doorbell events will appear here."
        );
    } catch (error) {
        console.error("Unable to clear activity:", error);
        window.alert("The images could not be cleared. Please try again.");
    } finally {
        button.disabled = false;
    }
}

async function sendDeleteRequest(url) {
    const response = await fetch(url, {
        method: "DELETE",
        headers: { Accept: "application/json" }
    });

    const data = await response.json().catch(() => ({}));
    if (!response.ok || !data.success) {
        throw new Error(data.error || `Request failed with status ${response.status}`);
    }
}

// Keep known event types visually distinct and handle future types safely.
function getEventInfo(eventType) {
    const normalizedType = String(eventType || "").toLowerCase();

    if (normalizedType === "motion") {
        return { label: "Motion Detected", badge: "Motion", icon: "◉", style: "motion" };
    }

    if (normalizedType === "doorbell") {
        return { label: "Doorbell Pressed", badge: "Doorbell", icon: "●", style: "doorbell" };
    }

    return { label: "Activity Detected", badge: "Activity", icon: "◆", style: "other" };
}

function renderMessage(container, heading, message) {
    const messageCard = document.createElement("div");
    messageCard.className = "message-card";

    const headingElement = document.createElement("strong");
    headingElement.textContent = heading;

    const messageElement = document.createElement("p");
    messageElement.textContent = message;

    messageCard.append(headingElement, messageElement);
    container.replaceChildren(messageCard);
}
