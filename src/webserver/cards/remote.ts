import {
    cardContractAllowInSubpage,
    cardContractCardLabel,
    cardContractDefaultConfig,
    cardContractDomains,
    cardContractHidden,
    cardContractPickerKey,
} from "../generated/card_contract";
import {
    configOptionValue,
    preserveCardColorOptions,
    setConfigOptionValue,
} from "../model/config_primitives";
import type { CardRegistry, CardUiServices } from "../application/card_registry";
import type { ControlsFieldsFeature } from "../application/controls_fields";

// Remote card: each modal key sends remote.send_command with the saved command.
// Keys without a command are hidden on the panel.
const REMOTE_PAD_KEYS: [string, string, string][] = [
    ["up", "Up", "up"],
    ["dn", "Down", "down"],
    ["lt", "Left", "left"],
    ["rt", "Right", "right"],
    ["ok", "OK", "select"],
];
const REMOTE_VOLUME_KEYS: [string, string, string][] = [
    ["vu", "Volume Up", "volume_up"],
    ["vd", "Volume Down", "volume_down"],
    ["mt", "Mute", "mute"],
];
const REMOTE_ROW_KEYS: [string, string, string][] = [
    ["bk", "Back", "menu"],
    ["hm", "Home", "home"],
    ["mn", "Menu", "top_menu"],
    ["rw", "Rewind", "skip_backward"],
    ["pp", "Play/Pause", "play_pause"],
    ["ff", "Fast Forward", "skip_forward"],
    ["pw", "Power", "suspend"],
];
const REMOTE_ROW_ORDER_OPTION = "ro";
// "touch" turns the direction keys into a touchpad: swipe to move, tap for OK.
const REMOTE_NAVIGATION_OPTION = "nv";
const REMOTE_DEFAULT_ROW_ORDER = REMOTE_ROW_KEYS.map(function (key) { return key[0]; });
const REMOTE_OPTION_NAMES = REMOTE_PAD_KEYS.concat(REMOTE_VOLUME_KEYS, REMOTE_ROW_KEYS)
    .map(function (key) { return key[0]; })
    .concat([REMOTE_ROW_ORDER_OPTION]);

function normalizedRowOrder(keys: string[]): string[] {
    var order: string[] = [];
    keys.forEach(function (key) {
        if (REMOTE_DEFAULT_ROW_ORDER.indexOf(key) >= 0 && order.indexOf(key) < 0)
            order.push(key);
    });
    REMOTE_DEFAULT_ROW_ORDER.forEach(function (key) {
        if (order.indexOf(key) < 0)
            order.push(key);
    });
    return order;
}

export function remoteRowOrder(options: unknown): string[] {
    return normalizedRowOrder(String(configOptionValue(options, REMOTE_ROW_ORDER_OPTION) || "").split("|"));
}

// The default order is omitted so unchanged cards stay compact.
export function setRemoteRowOrder(options: unknown, order: string[]): string {
    var value = normalizedRowOrder(order).join("|");
    return setConfigOptionValue(options, REMOTE_ROW_ORDER_OPTION, value === REMOTE_DEFAULT_ROW_ORDER.join("|") ? "" : value);
}

// Keep known remote keys in canonical order, plus any shared card colours.
export function normalizeRemoteOptions(options: unknown): string {
    var out = "";
    REMOTE_OPTION_NAMES.forEach(function (name) {
        var value = name === REMOTE_ROW_ORDER_OPTION ? "" : configOptionValue(options, name);
        if (value)
            out = setConfigOptionValue(out, name, value);
    });
    out = setRemoteRowOrder(out, remoteRowOrder(options));
    if (configOptionValue(options, REMOTE_NAVIGATION_OPTION) === "touch")
        out = setConfigOptionValue(out, REMOTE_NAVIGATION_OPTION, "touch");
    return preserveCardColorOptions(options, out);
}

export function registerRemoteCardTypes(
    registry: CardRegistry,
    fields: ControlsFieldsFeature,
    cardUi: CardUiServices,
): void {
    const { renderButtonSettings } = cardUi;
    const { cardBadgePreview } = fields;
    const REMOTE_CARD_METADATA: any = {
        entity: {
            label: "Remote Entity",
            idSuffix: "remote-entity",
            placeholder: "e.g. remote.living_room",
            domains: function (this: any) { return cardContractDomains("remote"); },
            bindName: "entity",
            rerender: true,
            requiredMessage: "Add a remote entity before saving.",
        },
        icon: {
            pickerIdSuffix: "remote-icon-picker",
            idSuffix: "remote-icon",
            field: "icon",
            fallback: "Remote TV",
            label: "Icon",
        },
        preview: {
            badge: "remote-tv",
        },
    };
    registry.register("remote", {
        label: function (this: any) { return cardContractCardLabel("remote"); },
        allowInSubpage: function (this: any) { return cardContractAllowInSubpage("remote"); },
        pickerKey: function (this: any) { return cardContractPickerKey("remote"); },
        hidden: function (this: any) { return cardContractHidden("remote"); },
        labelPlaceholder: "e.g. Living Room TV",
        defaultConfig: function (this: any) { return cardContractDefaultConfig("remote"); },
        cardMetadata: REMOTE_CARD_METADATA,
        normalizeConfig: function (this: any, b?: any) {
            b.sensor = "";
            b.unit = "";
            b.precision = "";
            b.icon_on = "Auto";
            b.options = normalizeRemoteOptions(b.options);
        },
        onSelect: function (this: any, b?: any) {
            var defaults: any = cardContractDefaultConfig("remote");
            var entity: any = String(b.entity || "").indexOf("remote.") === 0 ? b.entity : "";
            Object.keys(defaults).forEach(function (this: any, key?: any) {
                if (key !== "label")
                    b[key] = defaults[key];
            });
            b.entity = entity;
        },
        renderSettingsBeforeLabel: function (this: any, panel?: any, b?: any, slot?: any, helpers?: any) {
            helpers.renderCardEntityField(panel, b, helpers, REMOTE_CARD_METADATA);
        },
        renderSettings: function (this: any, panel?: any, b?: any, slot?: any, helpers?: any) {
            helpers.renderCardIconPicker(panel, b, helpers, REMOTE_CARD_METADATA.icon);
            var disclosure: any = helpers.disclosureSection("Remote Buttons", helpers.idPrefix + "remote-buttons", !!b._remoteButtonsOpen);
            var section: any = disclosure.section;
            panel.appendChild(disclosure.panel);
            disclosure.button.addEventListener("click", function (this: any) {
                b._remoteButtonsOpen = disclosure.panel.classList.contains("sp-open");
            });

            var hint: any = document.createElement("div");
            hint.className = "sp-field-hint";
            hint.textContent = "Type the command Home Assistant sends for each button. Leave a button empty to hide it.";
            section.appendChild(hint);

            function saveCommand(this: any, option?: any, value?: any) {
                b.options = setConfigOptionValue(b.options, option, String(value || "").trim());
                helpers.saveField("options", b.options);
                renderButtonSettings();
            }
            function bindCommandInput(this: any, input?: any, option?: any) {
                function save(this: any) {
                    if (String(input.value || "").trim() !== configOptionValue(b.options, option))
                        saveCommand(option, input.value);
                }
                input.addEventListener("change", save);
                input.addEventListener("blur", save);
                input.addEventListener("keydown", function (this: any, e?: any) {
                    if (e.key === "Enter") {
                        save();
                        this.blur();
                    }
                });
            }
            function renderGroup(this: any, title?: any, keys?: any) {
                var heading: any = document.createElement("div");
                heading.className = "sp-field";
                heading.appendChild(helpers.fieldLabel(title));
                section.appendChild(heading);
                keys.forEach(function (this: any, key?: any) {
                    var field: any = helpers.textField(key[1], helpers.idPrefix + "remote-" + key[0], configOptionValue(b.options, key[0]) || "", "e.g. " + key[2], null, false);
                    section.appendChild(field.field);
                    bindCommandInput(field.input, key[0]);
                });
            }
            var touchpad: boolean = configOptionValue(b.options, REMOTE_NAVIGATION_OPTION) === "touch";
            var navigation: any = helpers.selectField("Navigation", helpers.idPrefix + "remote-navigation", [
                ["pad", "Arrows"],
                ["touch", "Touchpad"],
            ], touchpad ? "touch" : "pad");
            section.appendChild(navigation.field);
            var navigationHint: any = document.createElement("div");
            navigationHint.className = "sp-field-hint";
            navigationHint.textContent = touchpad
                ? "Swipe on the touchpad to send Up, Down, Left or Right; a longer swipe sends several steps. Tap to send OK."
                : "Arrow keys around an OK button.";
            section.appendChild(navigationHint);
            navigation.select.addEventListener("change", function (this: any) {
                b.options = setConfigOptionValue(b.options, REMOTE_NAVIGATION_OPTION, this.value === "touch" ? "touch" : "");
                b._remoteButtonsOpen = true;
                helpers.saveField("options", b.options);
                renderButtonSettings();
            });
            renderGroup(touchpad ? "Touchpad Commands" : "Direction Pad", REMOTE_PAD_KEYS);
            renderGroup("Volume", REMOTE_VOLUME_KEYS);

            var rowHeading: any = document.createElement("div");
            rowHeading.className = "sp-field";
            rowHeading.appendChild(helpers.fieldLabel("Button Row"));
            section.appendChild(rowHeading);
            var list: any = document.createElement("div");
            list.className = "sp-light-tab-list";
            section.appendChild(list);
            var rowKeys: any = {};
            REMOTE_ROW_KEYS.forEach(function (this: any, key?: any) { rowKeys[key[0]] = key; });
            function saveOrderFromRows(this: any) {
                var order: any = Array.prototype.slice.call(list.querySelectorAll(".sp-light-tab-row")).map(function (this: any, row?: any) {
                    return row.getAttribute("data-key");
                });
                b.options = setRemoteRowOrder(b.options, order);
                b._remoteButtonsOpen = true;
                helpers.saveField("options", b.options);
                renderButtonSettings();
            }
            function moveRow(this: any, row?: any, direction?: any) {
                var sibling: any = direction < 0 ? row.previousElementSibling : row.nextElementSibling;
                if (!sibling)
                    return;
                if (direction < 0)
                    list.insertBefore(row, sibling);
                else
                    list.insertBefore(sibling, row);
                saveOrderFromRows();
            }
            remoteRowOrder(b.options).forEach(function (this: any, option?: any) {
                var key: any = rowKeys[option];
                var row: any = document.createElement("div");
                row.className = "sp-light-tab-row";
                row.setAttribute("data-key", option);
                var controls: any = document.createElement("div");
                controls.className = "sp-light-tab-controls";
                var moveUp: any = document.createElement("button");
                moveUp.type = "button";
                moveUp.className = "sp-light-tab-move mdi mdi-chevron-up";
                moveUp.setAttribute("aria-label", "Move " + key[1] + " up");
                var moveDown: any = document.createElement("button");
                moveDown.type = "button";
                moveDown.className = "sp-light-tab-move mdi mdi-chevron-down";
                moveDown.setAttribute("aria-label", "Move " + key[1] + " down");
                controls.appendChild(moveUp);
                controls.appendChild(moveDown);
                row.appendChild(controls);
                var inputId: any = helpers.idPrefix + "remote-" + option;
                var label: any = document.createElement("label");
                label.className = "sp-light-tab-label";
                label.htmlFor = inputId;
                label.textContent = key[1];
                row.appendChild(label);
                var input: any = document.createElement("input");
                input.type = "text";
                input.className = "sp-input sp-remote-command";
                input.id = inputId;
                input.value = configOptionValue(b.options, option) || "";
                input.placeholder = "e.g. " + key[2];
                row.appendChild(input);
                bindCommandInput(input, option);
                moveUp.addEventListener("click", function (this: any) {
                    b._remoteButtonsOpen = true;
                    moveRow(row, -1);
                });
                moveDown.addEventListener("click", function (this: any) {
                    b._remoteButtonsOpen = true;
                    moveRow(row, 1);
                });
                list.appendChild(row);
            });
        },
        renderPreview: function (this: any, b?: any, helpers?: any) {
            return cardBadgePreview(b, helpers, {
                label: b.label || b.entity || "Remote",
                iconFallback: "Remote TV",
                badge: REMOTE_CARD_METADATA.preview.badge,
            });
        },
    });
}
