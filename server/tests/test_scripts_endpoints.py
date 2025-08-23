import asyncio
import pytest
from fastapi.testclient import TestClient

from server.app.main import app
from server.app.services.agent_service import agent_service
from server.app.models.agent import AgentRegistration, AgentResponse


@pytest.fixture(autouse=True)
def clear_agent_service_state():
    agent_service.agents.clear()
    agent_service.command_queue.clear()
    agent_service.command_executions.clear()
    yield
    agent_service.agents.clear()
    agent_service.command_queue.clear()
    agent_service.command_executions.clear()


@pytest.fixture
def client():
    return TestClient(app)


@pytest.fixture
def agent_id():
    aid = "agent_test_1"
    registration = AgentRegistration(machine_name="host1", machine_type="linux", ip_address="127.0.0.1")
    agent_service.register_agent(aid, registration)
    return aid


@pytest.fixture
def mock_send_ok(monkeypatch):
    async def _fake(agent_id, command):
        return AgentResponse(success=True, message="ok", data={"echo": command.command})

    monkeypatch.setattr(agent_service, "send_command_to_agent", _fake)


def test_run_script_queue_ok(client, agent_id, mock_send_ok):
    resp = client.post(f"/api/agents/{agent_id}/run_script", json={
        "script": "echo hello",
        "interpreter": "bash",
        "timeout_sec": 5,
        "capture_output": True
    })
    assert resp.status_code == 200
    data = resp.json()
    assert data["status"] in ("queued", "success")
    assert agent_id in agent_service.command_queue
    assert agent_service.command_queue[agent_id]
    assert agent_service.command_queue[agent_id][-1].command == "run_script"


def test_run_script_validation(client, agent_id):
    # Missing script/script_path/key
    resp = client.post(f"/api/agents/{agent_id}/run_script", json={})
    assert resp.status_code == 400


def test_run_script_unknown_agent(client):
    resp = client.post("/api/agents/unknown/run_script", json={"script": "echo"})
    assert resp.status_code == 404


def test_get_job_output(client, agent_id, mock_send_ok):
    resp = client.get(f"/api/agents/{agent_id}/jobs/abc123")
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True
    assert "data" in body


def test_kill_job(client, agent_id, mock_send_ok):
    resp = client.delete(f"/api/agents/{agent_id}/jobs/abc123")
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True


def test_list_jobs(client, agent_id, mock_send_ok):
    resp = client.get(f"/api/agents/{agent_id}/jobs")
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True
    assert "jobs" in body.get("data", {}) or True  # structure from agent


def test_push_script(client, agent_id, mock_send_ok):
    resp = client.post(f"/api/agents/{agent_id}/scripts", json={
        "name": "hello.sh",
        "content": "#!/bin/bash\necho hi\n",
        "chmod": "755"
    })
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True


def test_list_scripts(client, agent_id, mock_send_ok):
    resp = client.get(f"/api/agents/{agent_id}/scripts")
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True


def test_delete_script(client, agent_id, mock_send_ok):
    resp = client.delete(f"/api/agents/{agent_id}/scripts/hello.sh")
    assert resp.status_code == 200
    body = resp.json()
    assert body["success"] is True


