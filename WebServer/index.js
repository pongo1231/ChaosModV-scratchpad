let effects = [];

$(document).ready(function () {
    let xmlHttp = new XMLHttpRequest();
    xmlHttp.open("GET", "https://gopong.dev:6910/fetch_effects", false);
    xmlHttp.send(null);

    if (!xmlHttp.responseText) {
        $("#effect_buttons").append("No effects enabled :(");
        return;
    }

    const json = JSON.parse(xmlHttp.responseText);

    if (!json) {
        $("#effect_buttons").append("No effects enabled :(");
        return;
    }

    for (let i = 0; i < json.length; i++) {
        $("#effect_buttons").append('<button class="mdl-button mdl-js-button mdl-button--raised mdl-button--colored chaos_button" id="' + json[i].id + '">' + json[i].name + '</button>');
        if (json[i].cooldown)
            $("#" + json[i].id).prop("disabled", true);
        effects.push({ id: json[i].id, name: json[i].name, cooldown: json[i].cooldown });
    }

    setInterval(function () {
        for (let i = 0; i < json.length; i++) {
            if (json[i].cooldown && !--json[i].cooldown) {
                $("#" + json[i].id).prop("disabled", false);
            }
        };
    }, 1000);

    $("#chaos_search").val("");
    $("#chaos_search").on('input', function () {
        const text = $("#chaos_search").val();
        for (let i = 0; i < json.length; i++) {
            if (!json[i].name.toLowerCase().includes(text.toLowerCase()))
                $("#" + json[i].id).hide();
            else
                $("#" + json[i].id).show();
        }
    });

    $(".chaos_button").click(function (event) {
        let xmlHttp = new XMLHttpRequest();
        xmlHttp.open("POST", "https://gopong.dev:6910/trigger_effect", false);
        xmlHttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
        xmlHttp.send("id=" + event.target.id);

        if (xmlHttp.status != 200) {
            window.location.reload();
            return;
        }

        const result = parseInt(xmlHttp.responseText);
        if (result < 1) // effect still in cooldown or there's no cooldown
            return;

        $("#" + event.target.id).prop("disabled", true);

        for (let i = 0; i < json.length; i++) {
            if (json[i].id == event.target.id) {
                json[i].cooldown = result;
                break;
            }
        }
    });
});