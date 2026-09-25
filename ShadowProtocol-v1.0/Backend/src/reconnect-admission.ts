import type { Pool } from 'pg';
import { createHash, randomBytes, randomUUID } from 'node:crypto';

const hash = (value: string) => createHash('sha256').update(value).digest('hex');
type Reservation = { matchId: string; roundNumber: number; slotIndex: number; reconnectToken: string };
type Session = { uid: string; region: string; build: string };
type Admission = { allocationId: string; matchId: string; connectToken: string; reconnectGrantId?: string; roundNumber?: number };

// Lock the live allocation and match through redemption. Release/completion cannot interleave.
export async function exchangeReconnect(pool: Pool, input: Reservation, session: Session) {
  const c = await pool.connect();
  try {
    await c.query('begin');
    const allocation = await c.query(`select sa.id,sa.server_id,sa.region,sa.connect_host,sa.connect_port,m.server_build
      from server_allocations sa join matches m on m.id=sa.match_id
      join game_server_nodes n on n.id=sa.node_id
      where sa.match_id=$1 and sa.user_id=$2 and sa.status='live' and m.ended_at is null
        and m.region=$3 and m.server_build=$4 and n.status='ready'
      order by sa.id limit 1 for update of sa,m`, [input.matchId,session.uid,session.region,session.build]);
    if (!allocation.rowCount) { await c.query('rollback'); return null; }
    const token = randomBytes(32).toString('base64url');
    const grantId = randomUUID();
    const slot = await c.query(`update match_player_slots set reconnect_token_hash=null,
      reconnect_grant_id=$6,reconnect_admission_hash=$7
      where match_id=$1 and round_number=$2 and slot_index=$3 and user_id=$4
        and connection_state='reconnecting' and reconnect_token_hash=$5 and reconnect_deadline>clock_timestamp()
      returning reconnect_deadline`, [input.matchId,input.roundNumber,input.slotIndex,session.uid,hash(input.reconnectToken),grantId,hash(token)]);
    if (!slot.rowCount) { await c.query('rollback'); return null; }
    await c.query('commit');
    const a = allocation.rows[0];
    return { allocationId:a.id,matchId:input.matchId,serverId:a.server_id,region:a.region,
      connectHost:a.connect_host,connectPort:a.connect_port,networkBuild:a.server_build,tickRate:60,
      connectToken:token,reconnectGrantId:grantId,expiresAt:new Date(slot.rows[0].reconnect_deadline).toISOString() };
  } catch (error) { await c.query('rollback'); throw error; } finally { c.release(); }
}

export async function admitReconnect(pool: Pool, input: Admission, serverId: string) {
  if (!input.reconnectGrantId || !input.roundNumber) return null;
  const c = await pool.connect();
  try {
    await c.query('begin');
    const allocation = await c.query(`select sa.user_id,sa.server_id,sa.region,m.server_build
      from server_allocations sa join matches m on m.id=sa.match_id
      join game_server_nodes n on n.id=sa.node_id
      where sa.id=$1 and sa.match_id=$2 and sa.server_id=$3 and sa.status='live'
        and m.ended_at is null and n.status='ready'
      for update of sa,m`, [input.allocationId,input.matchId,serverId]);
    if (!allocation.rowCount) { await c.query('rollback'); return null; }
    const a = allocation.rows[0];
    const slot = await c.query(`update match_player_slots set connection_state='connected',ready=false,
      reconnect_grant_id=null,reconnect_admission_hash=null,reconnect_deadline=null,reconnect_token_hash=null
      where match_id=$1 and round_number=$2 and user_id=$3 and reconnect_grant_id=$4
        and reconnect_admission_hash=$5 and connection_state='reconnecting' and reconnect_deadline>clock_timestamp()
      returning slot_index,round_number`, [input.matchId,input.roundNumber,a.user_id,input.reconnectGrantId,hash(input.connectToken)]);
    if (!slot.rowCount) { await c.query('rollback'); return null; }
    await c.query('commit');
    return { admitted:true,allocationId:input.allocationId,matchId:input.matchId,serverId:a.server_id,
      region:a.region,userId:a.user_id,networkBuild:a.server_build,reconnectGrantId:input.reconnectGrantId,
      roundNumber:slot.rows[0].round_number,slotIndex:slot.rows[0].slot_index,authority:'dedicated-server' };
  } catch (error) { await c.query('rollback'); throw error; } finally { c.release(); }
}
