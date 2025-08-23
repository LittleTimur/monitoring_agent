import os
import json
import shutil
import tempfile
import pytest

from pathlib import Path

from server.app.models.agent import AgentCommand


def test_user_parameter_substitution(tmp_path):
    # Minimal offline test to ensure server payload shaping
    key = "app.status[*]"
    params = ["svc", "brief"]
    cmd = AgentCommand(command="run_script", data={"key": key, "params": params})
    payload = cmd.dict()
    assert payload["data"]["key"] == key
    assert payload["data"]["params"] == params


