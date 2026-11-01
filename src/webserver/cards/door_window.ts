import {
    cardContractAllowInSubpage,
    cardContractCard,
    cardContractCardLabel,
    cardContractDefaultConfig,
    cardContractDomains,
    cardContractHidden,
    cardContractPickerKey,
} from "../generated/card_contract";
import { iconSlug } from "../application/ui_primitives";
import { configOptionEnabled, configOptionValue, setConfigOption, setConfigOptionValue } from "../model/config_primitives";
import type { CardRegistry, CardUiServices } from "../application/card_registry";
import type { ConfigSensorOptionsFeature } from "../application/config_sensor_options";
import type { ControlsFieldsFeature } from "../application/controls_fields";
export function registerDoorWindowCardTypes(
    registry: CardRegistry,
    sensorOptions: ConfigSensorOptionsFeature,
    fields: ControlsFieldsFeature,
    cardUi: CardUiServices,
): void {
    const { cardBadgePreview } = fields;
    const { renderPreview } = cardUi;
    const {
        normalizeDoorWindowSubtype,
        doorWindowActiveColorEnabled,
        normalizeDoorWindowOptions,
        doorWindowClosedIcon,
        doorWindowOpenIcon,
        setDoorWindowActiveColorEnabled,
    } = sensorOptions;
    // Read-only door/window card: shows a binary sensor with subtype-specific icons.
    const DOOR_WINDOW_CARD_METADATA: any = {
        mode: {
            label: "Type",
            idSuffix: "door-window-type",
            options: [
                ["door", "Door"],
                ["window", "Window"],
            ],
            value: function (this: any, b?: any) {
                return normalizeDoorWindowSubtype(b.precision);
            },
        },
        entity: {
            label: "Sensor Entity",
            idSuffix: "sensor",
            placeholder: "e.g. binary_sensor.patio_door",
            domains: function (this: any) { return cardContractDomains("door_window"); },
            bindName: "sensor",
            rerender: true,
            requiredMessage: "Add a door or window sensor before saving.",
        },
        labelField: {
            label: "Label",
            idSuffix: "label",
            field: "label",
            placeholder: "e.g. Patio Door",
            rerender: true,
        },
        activeColor: {
            label: "Lit When Open",
            idSuffix: "door-window-active-color",
            checked: doorWindowActiveColorEnabled,
        },
    };
    // Optional second sensor on the right half, stored in the card options.
    function renderSecondSensorSettings(this: any, panel?: any, b?: any, helpers?: any) {
        var hasSensor: any = !!configOptionValue(b.options, "sensor2");
        var disclosure: any = helpers.disclosureSection(
            "Right Sensor", helpers.idPrefix + "door-window-second", b._doorWindowSecondOpen === true || hasSensor);
        var section: any = disclosure.section;
        var hint: any = document.createElement("div");
        hint.className = "sp-field-hint";
        hint.textContent = "Add a second sensor to split the card. It shows on the right half and lights up on its own.";
        section.appendChild(hint);
        function saveOptions(this: any, rerender?: any) {
            b.options = normalizeDoorWindowOptions(b.options);
            b._doorWindowSecondOpen = true;
            helpers.saveField("options", b.options);
            if (rerender)
                renderPreview();
        }
        var typeField: any = helpers.selectField("Right Type", helpers.idPrefix + "door-window-type2",
            DOOR_WINDOW_CARD_METADATA.mode.options,
            normalizeDoorWindowSubtype(configOptionValue(b.options, "subtype2")));
        typeField.select.addEventListener("change", function (this: any) {
            b.options = setConfigOptionValue(b.options, "subtype2", typeField.select.value === "window" ? "window" : "");
            saveOptions(true);
        });
        section.appendChild(typeField.field);
        var sensorField: any = helpers.renderCardEntityField(section, b, helpers, {
            entity: {
                label: "Right Sensor Entity",
                idSuffix: "door-window-sensor2",
                value: function (this: any) { return configOptionValue(b.options, "sensor2"); },
                placeholder: "e.g. binary_sensor.kitchen_window",
                domains: function (this: any) { return cardContractDomains("door_window"); },
                bindName: null,
                rerender: false,
            },
        });
        function syncSensor(this: any) {
            var hadSensor: any = !!configOptionValue(b.options, "sensor2");
            b.options = setConfigOptionValue(b.options, "sensor2", sensorField.input.value);
            // Match the left sensor's default: a newly added sensor lights when open.
            if (!hadSensor && configOptionValue(b.options, "sensor2"))
                b.options = setConfigOption(b.options, "active_color2", true);
            saveOptions(true);
        }
        sensorField.input.addEventListener("change", syncSensor);
        sensorField.input.addEventListener("blur", syncSensor);
        var labelField: any = helpers.textField("Right Label", helpers.idPrefix + "door-window-label2",
            configOptionValue(b.options, "label2"), "e.g. Kitchen Window");
        labelField.input.addEventListener("input", function (this: any) {
            b.options = setConfigOptionValue(b.options, "label2", labelField.input.value);
            saveOptions(true);
        });
        section.appendChild(labelField.field);
        var litRow: any = helpers.toggleRow("Right Lit When Open", helpers.idPrefix + "door-window-active-color2",
            configOptionEnabled(b.options, "active_color2"));
        litRow.input.addEventListener("change", function (this: any) {
            b.options = setConfigOption(b.options, "active_color2", litRow.input.checked);
            saveOptions(true);
        });
        section.appendChild(litRow.row);
        panel.appendChild(disclosure.panel);
    }
    registry.register("door_window", {
        label: function (this: any) { return cardContractCardLabel("door_window"); },
        allowInSubpage: function (this: any) { return cardContractAllowInSubpage("door_window"); },
        pickerKey: function (this: any) { return cardContractPickerKey("door_window"); },
        hidden: function (this: any) { return cardContractHidden("door_window"); },
        hideLabel: true,
        defaultConfig: function (this: any) { return cardContractDefaultConfig("door_window"); },
        cardMetadata: DOOR_WINDOW_CARD_METADATA,
        onSelect: function (this: any, b?: any) {
            var defaults: any = cardContractDefaultConfig("door_window");
            Object.keys(defaults).forEach(function (this: any, key?: any) { b[key] = defaults[key]; });
        },
        renderSettings: function (this: any, panel?: any, b?: any, slot?: any, helpers?: any) {
            b.entity = "";
            b.unit = "";
            b.precision = normalizeDoorWindowSubtype(b.precision);
            b.options = normalizeDoorWindowOptions(b.options);
            if (!b.icon || b.icon === "Auto")
                b.icon = doorWindowClosedIcon(b.precision);
            if (!b.icon_on || b.icon_on === "Auto")
                b.icon_on = doorWindowOpenIcon(b.precision);
            var subtypeField: any = helpers.renderCardModeSelector(panel, b, helpers, Object.assign({}, DOOR_WINDOW_CARD_METADATA, {
                mode: Object.assign({}, DOOR_WINDOW_CARD_METADATA.mode, {
                    onChange: function (this: any) {
                        setSubtype(this.value, true);
                    },
                }),
            }));
            var subtypeSelect: any = subtypeField.select;
            helpers.renderBasicCardFields(panel, b, helpers, DOOR_WINDOW_CARD_METADATA);
            var iconPickers: any = helpers.renderCardIconPair(panel, b, helpers, {
                pickerIdSuffix: "closed-icon-picker",
                idSuffix: "icon",
                field: "icon",
                label: "Closed Icon",
                fallback: function (this: any) { return doorWindowClosedIcon(b.precision); },
            }, {
                pickerIdSuffix: "open-icon-picker",
                idSuffix: "icon-on",
                field: "icon_on",
                label: "Open Icon",
                fallback: function (this: any) { return doorWindowOpenIcon(b.precision); },
            });
            var closedIconPicker: any = iconPickers.off;
            var openIconPicker: any = iconPickers.on;
            helpers.renderCardActiveColorToggle(panel, b, helpers, DOOR_WINDOW_CARD_METADATA.activeColor, setDoorWindowActiveColorEnabled);
            function syncIconPicker(this: any, picker?: any, value?: any) {
                var preview: any = picker.querySelector(".sp-icon-picker-preview");
                if (preview)
                    preview.className = "sp-icon-picker-preview mdi mdi-" + iconSlug(value);
                var input: any = picker.querySelector(".sp-icon-picker-input");
                if (input)
                    input.value = value;
            }
            function setSubtype(this: any, value?: any, persist?: any) {
                var previousClosed: any = doorWindowClosedIcon(b.precision);
                var previousOpen: any = doorWindowOpenIcon(b.precision);
                b.precision = normalizeDoorWindowSubtype(value);
                subtypeSelect.value = b.precision;
                if (!b.icon || b.icon === "Auto" || b.icon === previousClosed) {
                    b.icon = doorWindowClosedIcon(b.precision);
                    syncIconPicker(closedIconPicker, b.icon);
                }
                if (!b.icon_on || b.icon_on === "Auto" || b.icon_on === previousOpen) {
                    b.icon_on = doorWindowOpenIcon(b.precision);
                    syncIconPicker(openIconPicker, b.icon_on);
                }
                if (!persist)
                    return;
                helpers.saveField("precision", b.precision);
                helpers.saveField("icon", b.icon);
                helpers.saveField("icon_on", b.icon_on);
            }
            setSubtype(b.precision, false);
            renderSecondSensorSettings(panel, b, helpers);
        },
        renderPreview: function (this: any, b?: any, helpers?: any) {
            var subtype: any = normalizeDoorWindowSubtype(b.precision);
            var label: any = b.label || b.sensor || (subtype === "window" ? "Window" : "Door");
            var sensor2: any = configOptionValue(b.options, "sensor2");
            if (sensor2) {
                // Split preview: the left sensor shows closed, the right one open.
                var subtype2: any = normalizeDoorWindowSubtype(configOptionValue(b.options, "subtype2"));
                var label2: any = configOptionValue(b.options, "label2") || sensor2;
                var leftIcon: any = b.icon && b.icon !== "Auto" ? b.icon : doorWindowClosedIcon(subtype);
                return {
                    buttonClass: "sp-door-window-split" +
                        (configOptionEnabled(b.options, "active_color2") ? " sp-door-window-right-lit" : ""),
                    iconHtml: '<span class="sp-door-window-half sp-door-window-half-right"></span>' +
                        '<span class="sp-btn-icon mdi mdi-' + iconSlug(leftIcon) + '"></span>' +
                        '<span class="sp-door-window-icon2 mdi mdi-' + iconSlug(doorWindowOpenIcon(subtype2)) + '"></span>',
                    labelHtml: '<span class="sp-btn-label-row sp-door-window-labels">' +
                        '<span class="sp-btn-label">' + helpers.escHtml(label) + '</span>' +
                        '<span class="sp-btn-label sp-door-window-label2">' + helpers.escHtml(label2) + '</span></span>',
                    labelEditable: false,
                };
            }
            return cardBadgePreview(b, helpers, {
                label: label,
                iconFallback: doorWindowClosedIcon(subtype),
                badge: subtype === "window" ? "window-closed" : "door",
            });
        },
    });
}
