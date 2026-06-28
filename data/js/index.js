

const ESP_IP = "http://192.168.1.71"; 

let dragging = false;

async function openBlind() {
  const response = await fetch(ESP_IP + "/open");
  const data = await response.json();

  showToast(data.status);
}
async function closeBlind()
{
    const response = await fetch(ESP_IP + "/close");
    const data = await response.json();

    showToast(data.status);
}

async function setPosition(val)
{
    const response = await fetch(ESP_IP + "/set?pos=" + val);
    const data = await response.json();

    showToast("Конечное положение шторы: " + data.position + "%");
}

async function loadStatus()
{
    if (dragging)
        return;

    const response = await fetch(ESP_IP + "/status");
    const data = await response.json();

    document.getElementById("slider").value = data.position;
    document.getElementById("value").innerText = data.position + "%";

    
}

async function saveSchedule() {
    const open = document.getElementById("openTime").value;
    const close = document.getElementById("closeTime").value;
    const once = document.getElementById("onceEvent").checked;

    

    document.getElementById("openTime").value = "";
    document.getElementById("closeTime").value = "";

    const response = await fetch(`${ESP_IP}/schedule?open=${encodeURIComponent(open)}&close=${encodeURIComponent(close)}&once=${once}`);

    await loadEvents();

    const data = await response.json();

    showToast(data.status);
    

}

function showToast(message)
{
    const toast = document.getElementById("toast");

    toast.innerText = message;
    toast.classList.add("show");

    setTimeout(() =>
    {
        toast.classList.remove("show");
    }, 5000);
}

async function loadEvents()
{
    const response = await fetch(ESP_IP + "/event");
    const data = await response.json();

    const list = document.getElementById("eventsList");
    list.innerHTML = "";

    if (!data || data.length === 0)
    {
        const empty = document.createElement("div");
        empty.innerText = "Нет запланированных событий";
        empty.style.opacity = "0.6";
        empty.style.padding = "10px";
        list.appendChild(empty);
        return;
    }

    data.forEach(ev =>
    {
        const div = document.createElement("div");

        div.innerHTML = `
            <span>
                ${ev.time} — ${ev.open ? "Открыть" : "Закрыть"} ${ev.once ? "(однократно)" : "(ежедневно)"}
            </span>
            <button onclick="deleteEvent('${ev.time}')">Удалить</button>
        `;

        list.appendChild(div);
    });
}

async function deleteEvent(time)
{
    const response = await fetch(`${ESP_IP}/event/delete?time=${time}`);
    await loadEvents();
    const data = await response.json();

    showToast(data.status);
}

window.onload = () =>
{
    const slider = document.getElementById("slider");

    // ПК
    slider.addEventListener("mousedown", () =>
    {
        dragging = true;
    });

    slider.addEventListener("mouseup", () =>
    {
        dragging = false;
    });

    // Телефон
    slider.addEventListener("touchstart", () =>
    {
        dragging = true;
    });

    slider.addEventListener("touchend", () =>
    {
        dragging = false;
    });

    loadStatus();
    loadEvents();
};

setInterval(loadStatus, 500);
