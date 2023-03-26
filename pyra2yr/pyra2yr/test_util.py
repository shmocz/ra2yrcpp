import gzip

from pyra2yr.util import StateUtil, read_protobuf_messages


def verify_recording(path: str):
    n_deploy = 0
    last_frame = 0
    with gzip.open(path, "rb") as f:
        m = read_protobuf_messages(f)

        # get type classes
        m0 = next(m)
        S = StateUtil(m0.object_types)
        S.set_state(m0)
        last_frame = S.state.current_frame
        for _, m0 in enumerate(m):
            S.set_state(m0)
            for u in S.get_units("player_0"):
                if u.deployed or u.deploying:
                    n_deploy += 1
            cur_frame = S.state.current_frame
            assert (
                cur_frame - last_frame == 1
            ), f"cur,last={cur_frame},{last_frame}"
            last_frame = S.state.current_frame
    assert last_frame > 0
    assert n_deploy > 0
