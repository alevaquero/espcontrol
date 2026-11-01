"use strict";
const assert = require("node:assert/strict");
const { test } = require("node:test");
const path = require("node:path");
const { loadTypeScriptModule } = require("../../../scripts/load_typescript_module");
const root = path.resolve(__dirname, "../../..");
const {
  normalizeRemoteOptions,
  registerRemoteCardTypes,
  remoteRowOrder,
  setRemoteRowOrder,
} = loadTypeScriptModule(path.join(root, "src/webserver/cards/remote.ts"));

test("remote row order falls back to defaults and stays compact", () => {
  assert.deepEqual(remoteRowOrder(""), ["bk", "hm", "mn", "rw", "pp", "ff", "pw"]);
  assert.deepEqual(remoteRowOrder("ro=pw%7Cbad%7Cbk%7Cpw"), ["pw", "bk", "hm", "mn", "rw", "pp", "ff"]);
  assert.equal(setRemoteRowOrder("up=up", ["bk", "hm", "mn", "rw", "pp", "ff", "pw"]), "up=up");
  assert.equal(setRemoteRowOrder("up=up", ["pw", "bk"]), "up=up,ro=pw%7Cbk%7Chm%7Cmn%7Crw%7Cpp%7Cff");
});

test("remote options keep commands and card colours but drop unknown options", () => {
  assert.equal(
    normalizeRemoteOptions("junk=1,ok=DPAD_CENTER,up=DPAD_UP,ro=pp,card_off_color=112233"),
    "up=DPAD_UP,ok=DPAD_CENTER,ro=pp%7Cbk%7Chm%7Cmn%7Crw%7Cff%7Cpw,card_off_color=112233",
  );
});

test("selecting a remote card fills Apple TV commands and keeps a remote entity", () => {
  let remote;
  registerRemoteCardTypes({ register: (_, definition) => remote = definition }, { cardBadgePreview: () => ({}) }, {});
  const fromSwitch = { entity: "switch.tv", label: "TV", options: "confirm_message=x" };
  remote.onSelect(fromSwitch);
  assert.equal(fromSwitch.entity, "");
  assert.equal(fromSwitch.label, "TV");
  assert.match(fromSwitch.options, /^up=up,dn=down,.*pw=suspend$/);
  const fromRemote = { entity: "remote.living_room" };
  remote.onSelect(fromRemote);
  assert.equal(fromRemote.entity, "remote.living_room");
});
