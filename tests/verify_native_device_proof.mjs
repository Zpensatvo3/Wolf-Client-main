import { verifyDeviceProof } from "../server/device-proof.mjs";
let input = "";
for await (const chunk of process.stdin) { input += chunk; if (input.length > 4096) throw new Error("Oversized probe data"); }
const body = JSON.parse(input);
await verifyDeviceProof(body);
console.log("Native Windows device signature verified by server code; hardware-backed:", body.hardware_backed);
