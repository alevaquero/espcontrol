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
        normalizeDoorWindowSides,
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
    var DOOR_WINDOW_RIGHT_OPTIONS: any = ["sensor2", "label2", "subtype2", "icon2", "icon_on2", "active_color2"];
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
            var sides: any = normalizeDoorWindowSides(b.options);
            function syncIconPicker(this: any, picker?: any, value?: any) {
                var preview: any = picker.querySelector(".sp-icon-picker-preview");
                if (preview)
                    preview.className = "sp-icon-picker-preview mdi mdi-" + iconSlug(value);
                var input: any = picker.querySelector(".sp-icon-picker-input");
                if (input)
                    input.value = value;
            }
            // Show: which halves of the card have a sensor.
            helpers.renderCardSegmentControl(panel, b, helpers, {
                label: "Show",
                primary: true,
                options: [["left", "Left"], ["right", "Right"], ["both", "Both"]],
                value: function (this: any) { return sides; },
                onSelect: function (this: any, _b?: any, _helpers?: any, value?: any) {
                    // Keep the right sensor's settings while it is hidden so it
                    // can be shown again without re-entering them.
                    if (value === "left" && sides !== "left") {
                        b._doorWindowRightOptions = {};
                        DOOR_WINDOW_RIGHT_OPTIONS.forEach(function (this: any, key?: any) {
                            b._doorWindowRightOptions[key] = key === "active_color2"
                                ? configOptionEnabled(b.options, key) : configOptionValue(b.options, key);
                        });
                    }
                    else if (value !== "left" && sides === "left" && b._doorWindowRightOptions) {
                        DOOR_WINDOW_RIGHT_OPTIONS.forEach(function (this: any, key?: any) {
                            var saved: any = b._doorWindowRightOptions[key];
                            b.options = key === "active_color2"
                                ? setConfigOption(b.options, key, saved) : setConfigOptionValue(b.options, key, saved);
                        });
                        b._doorWindowRightOptions = null;
                    }
                    sides = value;
                    b.options = setConfigOptionValue(b.options, "sides", sides);
                    saveOptions();
                    syncSides();
                },
            });
            function saveOptions(this: any) {
                b.options = normalizeDoorWindowOptions(b.options);
                helpers.saveField("options", b.options);
                renderPreview();
            }

            // Left half: the card's own sensor fields.
            var left: any = helpers.disclosureSection("Left Card Settings", helpers.idPrefix + "door-window-left",
                sides !== "right" && !b.sensor);
            var leftSection: any = left.section;
            var subtypeField: any = helpers.selectField("Type", helpers.idPrefix + DOOR_WINDOW_CARD_METADATA.mode.idSuffix,
                DOOR_WINDOW_CARD_METADATA.mode.options, b.precision, function (this: any) {
                    setSubtype(this.value, true);
                });
            leftSection.appendChild(subtypeField.field);
            var leftEntity: any = DOOR_WINDOW_CARD_METADATA.entity;
            var sensorField: any = helpers.entityField(leftEntity.label, helpers.idPrefix + leftEntity.idSuffix, b.sensor || "",
                leftEntity.placeholder, cardContractDomains("door_window"), "sensor", true);
            helpers.requireField(sensorField.input, leftEntity.requiredMessage, function (this: any) { return sides !== "right"; });
            leftSection.appendChild(sensorField.field);
            helpers.renderCardTextField(leftSection, b, helpers, DOOR_WINDOW_CARD_METADATA.labelField);
            var iconPickers: any = helpers.renderCardIconPair(leftSection, b, helpers, {
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
            helpers.renderCardActiveColorToggle(leftSection, b, helpers, DOOR_WINDOW_CARD_METADATA.activeColor, setDoorWindowActiveColorEnabled);
            panel.appendChild(left.panel);
            function setSubtype(this: any, value?: any, persist?: any) {
                var previousClosed: any = doorWindowClosedIcon(b.precision);
                var previousOpen: any = doorWindowOpenIcon(b.precision);
                b.precision = normalizeDoorWindowSubtype(value);
                subtypeField.select.value = b.precision;
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
                renderPreview();
            }
            setSubtype(b.precision, false);

            // Right half: the second sensor, stored in the card options. Its
            // icons follow its type unless the user picks different ones.
            var right: any = helpers.disclosureSection("Right Card Settings", helpers.idPrefix + "door-window-right",
                sides !== "left" && !configOptionValue(b.options, "sensor2"));
            var rightSection: any = right.section;
            function subtype2(this: any) {
                return normalizeDoorWindowSubtype(configOptionValue(b.options, "subtype2"));
            }
            function rightIcon(this: any, key?: any) {
                return configOptionValue(b.options, key) ||
                    (key === "icon2" ? doorWindowClosedIcon(subtype2()) : doorWindowOpenIcon(subtype2()));
            }
            var typeField: any = helpers.selectField("Type", helpers.idPrefix + "door-window-type2",
                DOOR_WINDOW_CARD_METADATA.mode.options, subtype2(), function (this: any) {
                    b.options = setConfigOptionValue(b.options, "subtype2", typeField.select.value === "window" ? "window" : "");
                    syncIconPicker(closedIcon2Picker, rightIcon("icon2"));
                    syncIconPicker(openIcon2Picker, rightIcon("icon_on2"));
                    saveOptions();
                });
            rightSection.appendChild(typeField.field);
            var sensor2Field: any = helpers.entityField(leftEntity.label, helpers.idPrefix + "door-window-sensor2",
                configOptionValue(b.options, "sensor2"), "e.g. binary_sensor.kitchen_window",
                cardContractDomains("door_window"), null, false);
            helpers.requireField(sensor2Field.input, leftEntity.requiredMessage, function (this: any) { return sides !== "left"; });
            function syncSensor2(this: any) {
                var hadSensor: any = !!configOptionValue(b.options, "sensor2");
                b.options = setConfigOptionValue(b.options, "sensor2", sensor2Field.input.value);
                // Match the left sensor's default: a newly added sensor lights when open.
                if (!hadSensor && configOptionValue(b.options, "sensor2")) {
                    b.options = setConfigOption(b.options, "active_color2", true);
                    lit2Row.input.checked = true;
                }
                saveOptions();
            }
            sensor2Field.input.addEventListener("change", syncSensor2);
            sensor2Field.input.addEventListener("blur", syncSensor2);
            rightSection.appendChild(sensor2Field.field);
            var label2Field: any = helpers.textField("Label", helpers.idPrefix + "door-window-label2",
                configOptionValue(b.options, "label2"), "e.g. Kitchen Window");
            label2Field.input.addEventListener("input", function (this: any) {
                b.options = setConfigOptionValue(b.options, "label2", label2Field.input.value);
                saveOptions();
            });
            rightSection.appendChild(label2Field.field);
            function rightIconPicker(this: any, key?: any, idSuffix?: any, label?: any) {
                var picker: any = helpers.iconPickerField(helpers.idPrefix + idSuffix + "-picker", helpers.idPrefix + idSuffix,
                    rightIcon(key), function (this: any, opt?: any) {
                        var fallback: any = key === "icon2" ? doorWindowClosedIcon(subtype2()) : doorWindowOpenIcon(subtype2());
                        // Store only a custom choice so the icon keeps following the type.
                        b.options = setConfigOptionValue(b.options, key, !opt || opt === "Auto" || opt === fallback ? "" : opt);
                        saveOptions();
                    }, label);
                rightSection.appendChild(picker);
                return picker;
            }
            var closedIcon2Picker: any = rightIconPicker("icon2", "door-window-icon2", "Closed Icon");
            var openIcon2Picker: any = rightIconPicker("icon_on2", "door-window-icon-on2", "Open Icon");
            var lit2Row: any = helpers.toggleRow("Lit When Open", helpers.idPrefix + "door-window-active-color2",
                configOptionEnabled(b.options, "active_color2"));
            lit2Row.input.addEventListener("change", function (this: any) {
                b.options = setConfigOption(b.options, "active_color2", lit2Row.input.checked);
                saveOptions();
            });
            rightSection.appendChild(lit2Row.row);
            panel.appendChild(right.panel);
            function syncSides(this: any) {
                left.panel.style.display = sides === "right" ? "none" : "";
                right.panel.style.display = sides === "left" ? "none" : "";
            }
            syncSides();
        },
        renderPreview: function (this: any, b?: any, helpers?: any) {
            var subtype: any = normalizeDoorWindowSubtype(b.precision);
            var label: any = b.label || b.sensor || (subtype === "window" ? "Window" : "Door");
            var sides: any = normalizeDoorWindowSides(b.options);
            if (sides !== "left") {
                // Split preview: the left sensor shows closed, the right one open.
                var showLeft: any = sides === "both";
                var sensor2: any = configOptionValue(b.options, "sensor2");
                var subtype2: any = normalizeDoorWindowSubtype(configOptionValue(b.options, "subtype2"));
                var label2: any = configOptionValue(b.options, "label2") || sensor2 || (subtype2 === "window" ? "Window" : "Door");
                var leftIcon: any = b.icon && b.icon !== "Auto" ? b.icon : doorWindowClosedIcon(subtype);
                var rightIcon: any = configOptionValue(b.options, "icon_on2") || doorWindowOpenIcon(subtype2);
                return {
                    buttonClass: "sp-door-window-split" +
                        (configOptionEnabled(b.options, "active_color2") ? " sp-door-window-right-lit" : ""),
                    iconHtml: '<span class="sp-door-window-half sp-door-window-half-right"></span>' +
                        (showLeft ? '<span class="sp-btn-icon mdi mdi-' + iconSlug(leftIcon) + '"></span>' : "") +
                        '<span class="sp-door-window-icon2 mdi mdi-' + iconSlug(rightIcon) + '"></span>',
                    labelHtml: '<span class="sp-btn-label-row sp-door-window-labels">' +
                        '<span class="sp-btn-label">' + (showLeft ? helpers.escHtml(label) : "") + '</span>' +
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
