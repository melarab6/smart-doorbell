// Load the latest doorbell events after the document is ready.
document.addEventListener("DOMContentLoaded", loadActivity);

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
    image.src = `/uploads/${encodeURIComponent(event.image_filename)}`;
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

    const badge = document.createElement("span");
    badge.className = "event-type-badge";
    badge.textContent = eventInfo.badge;

    textGroup.append(label, timestamp);
    details.append(textGroup, badge);
    card.append(imageWrap, details);

    return card;
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
