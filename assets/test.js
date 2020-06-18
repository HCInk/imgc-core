const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
// FUNCTIONS //HELPERS
const execCheck = (tool, args, file) => {
  const execArgs = args.split(",");
  execArgs.push(file);
  const out = execFileSync(tool, execArgs);
  return out.toString("utf-8");
};
const generateJTestReport = (results, time) => {
  const errors = results.filter((e) => !e.success).length;
  const successes = results.length - errors;
  return `<testsuites version="1">
<testsuite name="Compare Video Hashes" tests="${
    results.length
  }" failures="${errors}" errors="${errors}" timestamp="${new Date().toISOString()}" time="${time}">
  ${results
    .map((result) => {
      if (result.success) {
        return ` <testcase classname="${result.key.replace(
          ":",
          "."
        )}" name="Success ${result.key}" time="${result.time}"/>`;
      } else {
        return `<testcase classname="${result.key.replace(
          ":",
          "."
        )}" name="Fail ${result.key}" time="${result.time}">
        <failure type="HashCompareError"> ${result.reason}</failure>
    </testcase>`;
      }
    })
    .join(`\n`)}
</testsuite>
</testsuites>`;
};

//CMD logic

const options = {
  rendered_root: "",
  test_tool: "shasum",
  tool_args: "-a,256",
  action: "compare",
  reference_file: "preset.json",
};
process.argv.forEach((elem, index) => {
  switch (elem) {
    case "-r":
    case "--root": {
      options.rendered_root = process.argv[index + 1];
      break;
    }
    case "-t":
    case "--tool": {
      options.test_tool = process.argv[index + 1];
      break;
    }
    case "--tool-args": {
      options.tool_args = process.argv[index + 1];
      break;
    }
    case "-f":
    case "--ref": {
      options.reference_file = process.argv[index + 1];
      break;
    }
    case "-a":
    case "--action": {
      options.action = process.argv[index + 1];
      break;
    }
    default:
      break;
  }
});
//actual code logic
if (options.action === "generate") {
  if (!fs.existsSync(options.rendered_root)) {
    console.log("Root doesnt exists, aborting");
    process.exit(1);
  }
  const tree = {};
  const rootSet = fs.readdirSync(options.rendered_root);
  for (const entry of rootSet) {
    if (entry === ".DS_Store") {
      // MACOS stupidity
      continue;
    }

    const subPath = path.join(options.rendered_root, entry);
    const files = [];
    for (const file of fs.readdirSync(subPath)) {
      if (file === ".DS_Store") {
        // MACOS stupidity
        continue;
      }
      const hash = execCheck(
        options.test_tool,
        options.tool_args,
        path.join(subPath, file)
      ).substr(0, 64);
      files.push({ name: file, hash });
    }
    tree[entry] = files;
  }
  fs.writeFileSync(options.reference_file, JSON.stringify(tree));
} else {
  const start = Date.now();
  const tree = JSON.parse(fs.readFileSync(options.reference_file, "utf-8"));
  const results = [];
  for (const entryKey of Object.keys(tree)) {
    const entry = tree[entryKey];
    const entryPath = path.join(options.rendered_root, entryKey);
    if (!fs.existsSync(entryPath)) {
      entry.forEach((e) => {
        results.push({
          key: `${entryKey}:${e.name}`,
          reason: "Section fs directory was not found or isnt accesible",
          success: false,
          time: 0,
        });
      });
      continue;
    }
    for (const file of entry) {
      const subStart = Date.now();
      const filePath = path.join(entryPath, file.name);
      if (!fs.existsSync(filePath)) {
        const subEnd = Date.now();
        results.push({
          key: `${entryKey}:${file.name}`,
          reason: "file was not found or is not accesible",
          success: false,
          time: (subEnd - subStart) / 1000,
        });
        continue;
      }
      const hash = execCheck(
        options.test_tool,
        options.tool_args,
        filePath
      ).substr(0, 64);
      const subEnd = Date.now();
      if (hash !== file.hash)
        results.push({
          key: `${entryKey}:${file.name}`,
          reason: `Hash missmatch, expected: "${
            file.hash
          }", actual: "${hash}". Tool: ${
            options.test_tool
          }, args: ${options.tool_args.split(",")}, searched path: ${filePath}`,
          success: false,
          time: (subEnd - subStart) / 1000,
        });
      else
        results.push({
          key: `${entryKey}:${file.name}`,
          reason: `Hashes are equal!`,
          success: true,
          time: (subEnd - subStart) / 1000,
        });
    }
  }
  const end = Date.now();
  fs.mkdirSync("results");
  fs.writeFileSync(path.join("reslts", "result.xml"), generateJTestReport(results, (end - start) / 1000));
}
