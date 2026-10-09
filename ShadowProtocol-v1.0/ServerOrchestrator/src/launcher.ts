import { spawn, type ChildProcess } from 'node:child_process';
import { buildLaunchPlan } from './launch-plan.js';

function forwardSignal(child: ChildProcess, signal: NodeJS.Signals) {
  if (child.exitCode === null && !child.killed) child.kill(signal);
}

export function launchDedicatedServer(env: NodeJS.ProcessEnv = process.env): ChildProcess {
  const plan = buildLaunchPlan(env);
  process.stdout.write(
    `Launching Shadow Protocol server ${plan.identity.serverId} in ${plan.identity.region} at ${plan.identity.publicHost}:${plan.identity.publicPort}\n`
  );

  const child = spawn(plan.executable, plan.args, {
    env: plan.childEnv,
    shell: false,
    stdio: 'inherit'
  });

  const onSigTerm = () => forwardSignal(child, 'SIGTERM');
  const onSigInt = () => forwardSignal(child, 'SIGINT');
  process.once('SIGTERM', onSigTerm);
  process.once('SIGINT', onSigInt);

  child.once('exit', (code, signal) => {
    process.removeListener('SIGTERM', onSigTerm);
    process.removeListener('SIGINT', onSigInt);
    if (signal) {
      process.stderr.write(`Dedicated server exited from signal ${signal}\n`);
      process.exitCode = 1;
    } else {
      process.exitCode = code ?? 1;
    }
  });

  child.once('error', error => {
    process.stderr.write(`Dedicated server launch failed: ${error.message}\n`);
    process.exitCode = 1;
  });

  return child;
}

if (process.env.NODE_ENV !== 'test') {
  launchDedicatedServer();
}
